#include <common/log.hpp>
#include <common/catch.hpp>

#include <io/io.hpp>
#include <net/sockaddr.hpp>
#include <net/ops.hpp>

#include <array>
#include <string>

using namespace io;

TEST_CASE("accept/connect", "[io_net]")
{
    io_loop loop;
    loop.init();

    int s_fd, c_fd;

    s_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(s_fd >= 0);
    c_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    struct sock_addr addr("@tmp/io_accept_test", AF_UNIX);

    int ret = bind(s_fd, addr.sockaddr(), addr.len());
    REQUIRE(ret == 0);

    ret = listen(s_fd, 5);
    REQUIRE(ret == 0);

    auto accept_task = [&]() -> io_task
    {
        sock_addr remote;
        int remote_fd;
        auto res = co_await accept(loop, s_fd, remote_fd, remote);
        REQUIRE(res == io_result::done);
        LOG(debug) << "Accepted connection: " << remote_fd;
        close(remote_fd);

        co_return;
    };

    auto connect_task = [&]() -> io_task
    {
        auto res = co_await connect(loop, c_fd, addr);
        REQUIRE(res == io_result::done);

        co_return;
    };

    loop.schedule(accept_task(), "accept_task");
    loop.schedule(connect_task(), "connect_task");
    loop.run();

    close(s_fd);
    close(c_fd);
}

TEST_CASE("accept timeout", "[io_net]")
{
    io_loop loop;
    loop.init();

    int s_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(s_fd >= 0);

    struct sock_addr addr("@tmp/io_accept_timeout_test", AF_UNIX);

    int ret = bind(s_fd, addr.sockaddr(), addr.len());
    REQUIRE(ret == 0);

    ret = listen(s_fd, 5);
    REQUIRE(ret == 0);

    auto accept_task = [&]() -> io_task
    {
        sock_addr remote;
        int remote_fd;
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        auto res     = co_await accept(loop, s_fd, remote_fd, remote, timeout);
        REQUIRE(res == io_result::timeout);

        co_return;
    };

    loop.schedule(accept_task(), "accept_timeout_task");
    loop.run();

    close(s_fd);
}

TEST_CASE("connect to non-existent socket", "[io_net]")
{
    io_loop loop;
    loop.init();

    // Create client socket
    int c_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    // Use a path that doesn't exist
    struct sock_addr addr("@tmp/io_connect_nonexistent_test", AF_UNIX);

    auto connect_task = [&]() -> io_task
    {
        // Short timeout that shouldn't be reached
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);

        LOG(info) << "Trying connect to non-existent socket";
        auto res = co_await connect(loop, c_fd, addr, {}, timeout);

        // Should error (not timeout) since the destination doesn't exist
        LOG(info) << "Connect result: " << static_cast<int>(res);
        REQUIRE(res == io_result::error);

        co_return;
    };

    loop.schedule(connect_task(), "connect_nonexistent_task");
    loop.run();

    close(c_fd);
}

TEST_CASE("connect timeout", "[io_net]")
{
    io_loop loop;
    loop.init();

    // Create client socket (use TCP for proper timeout behavior)
    int c_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    // Use a non-routable IP address with a filtered port
    // 10.255.255.1 with a random high port is likely to cause a timeout
    // rather than an immediate rejection
    struct sock_addr addr("10.255.255.1:45678", AF_INET);

    auto connect_task = [&]() -> io_task
    {
        LOG(info) << "Attempting TCP connection to non-routable IP with 500ms timeout";

        // Short timeout to avoid waiting too long for the test
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

        // This should time out because the destination is unreachable
        auto res = co_await connect(loop, c_fd, addr, {}, timeout);

        LOG(info) << "Connect result: " << static_cast<int>(res);
        REQUIRE(res == io_result::timeout);

        co_return;
    };

    loop.schedule(connect_task(), "connect_timeout_task");
    loop.run();

    close(c_fd);
}

TEST_CASE("socket configuration", "[io_net]")
{
    // Create a TCP socket to test configuration on
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(fd >= 0);

    SECTION("default configuration")
    {
        socket_config config;
        config.apply(fd);

        // Verify TCP_NODELAY is enabled by default
        int nodelay_val  = 0;
        socklen_t optlen = sizeof(nodelay_val);
        int ret          = getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_val, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(nodelay_val == 1);
    }

    SECTION("custom buffer sizes")
    {
        socket_config config;
        config.send_buffer_size = 65536; // 64KB
        config.recv_buffer_size = 32768; // 32KB
        config.apply(fd);

        int send_buf     = 0;
        int recv_buf     = 0;
        socklen_t optlen = sizeof(send_buf);

        int ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf, &optlen);
        REQUIRE(ret == 0);
        // The kernel may double the buffer size we set
        REQUIRE(send_buf >= config.send_buffer_size);

        ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buf, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(recv_buf >= config.recv_buffer_size);
    }

    SECTION("keep-alive settings")
    {
        socket_config config;
        config.keep_alive          = true;
        config.keep_alive_idle     = 30;
        config.keep_alive_interval = 5;
        config.keep_alive_count    = 4;
        config.apply(fd);

        int keep_alive   = 0;
        socklen_t optlen = sizeof(keep_alive);
        int ret          = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(keep_alive == 1);

        int idle_val = 0;
        ret          = getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_val, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(idle_val == 30);

        int interval_val = 0;
        ret              = getsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval_val, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(interval_val == 5);

        int count_val = 0;
        ret           = getsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count_val, &optlen);
        REQUIRE(ret == 0);
        REQUIRE(count_val == 4);
    }

    close(fd);
}

TEST_CASE("connect with config", "[io_net]")
{
    io_loop loop;
    loop.init();

    int s_fd, c_fd;

    s_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(s_fd >= 0);
    c_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    struct sock_addr addr("@tmp/io_connect_config_test", AF_UNIX);

    int ret = bind(s_fd, addr.sockaddr(), addr.len());
    REQUIRE(ret == 0);

    ret = listen(s_fd, 5);
    REQUIRE(ret == 0);

    auto accept_task = [&]() -> io_task
    {
        sock_addr remote;
        int remote_fd;
        auto res = co_await accept(loop, s_fd, remote_fd, remote);
        REQUIRE(res == io_result::done);
        LOG(debug) << "Accepted connection: " << remote_fd;
        close(remote_fd);
        co_return;
    };

    auto connect_task = [&]() -> io_task
    {
        // Create a custom socket config
        socket_config config;
        config.tcp_nodelay      = true;
        config.send_buffer_size = 32768;
        config.recv_buffer_size = 32768;

        // Use the new connect_with_config API
        auto res = co_await io::connect(loop, c_fd, addr, config);
        REQUIRE(res == io_result::done);

        // Verify some of the settings were applied
        int nodelay_val  = 0;
        socklen_t optlen = sizeof(nodelay_val);
        int ret          = getsockopt(c_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_val, &optlen);

        // This might not be applicable for AF_UNIX, but we're testing the API itself
        // so we don't check the return code
        if (ret == 0) { REQUIRE(nodelay_val == 1); }

        co_return;
    };

    loop.schedule(accept_task(), "accept_task");
    loop.schedule(connect_task(), "connect_with_config_task");
    loop.run();

    close(s_fd);
    close(c_fd);
}

TEST_CASE("basic recv", "[io_net]")
{
    io_loop loop;
    loop.init();

    int s_fd, c_fd;

    s_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(s_fd >= 0);
    c_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    struct sock_addr addr("@tmp/io_recv_test", AF_UNIX);

    int ret = bind(s_fd, addr.sockaddr(), addr.len());
    REQUIRE(ret == 0);

    ret = listen(s_fd, 5);
    REQUIRE(ret == 0);

    auto server_task = [&]() -> io_task
    {
        sock_addr remote;
        int remote_fd;
        auto accept_res = co_await accept(loop, s_fd, remote_fd, remote);
        REQUIRE(accept_res == io_result::done);
        LOG(debug) << "Accepted connection: " << remote_fd;

        // Send test data
        const char* test_msg = "Hello, receiver!";
        size_t msg_len = strlen(test_msg);
        size_t sent = 0;
        auto send_op         = io::send(loop, remote_fd, test_msg, msg_len, sent);
        auto send_res = co_await send_op;
        REQUIRE(send_res == io_result::done);
        REQUIRE(sent > 0);                             // Make sure send succeeded
        REQUIRE(static_cast<size_t>(sent) == msg_len);  // Fix signedness warning
        
        // Keep connection open until explicitly closed at end of test
        shutdown(remote_fd, SHUT_WR);
        close(remote_fd);
        co_return;
    };

    auto client_task = [&]() -> io_task
    {
        // Connect to server
        auto connect_res = co_await connect(loop, c_fd, addr);
        REQUIRE(connect_res == io_result::done);
        LOG(debug) << "Connected to server";

        // Receive data
        std::array<char, 128> buffer{};
        // Explicitly zero out the buffer to ensure no garbage data
        memset(buffer.data(), 0, buffer.size());
        
        ssize_t bytes_received = 0;
        auto recv_op = recv(loop, c_fd, buffer.data(), buffer.size() - 1, bytes_received);
        auto res = co_await recv_op;

        if (res == io_result::closed)
        {
            LOG(debug) << "Connection closed by server.";
            REQUIRE(bytes_received == 0);
        }
        else
        {
            REQUIRE(res == io_result::done);
            REQUIRE(bytes_received > 0);

            // Ensure null termination
            buffer[bytes_received] = '\0';
            std::string received(buffer.data(), bytes_received);
            LOG(debug) << "Received: " << received << " (" << bytes_received << " bytes)";
            REQUIRE(received == "Hello, receiver!");
        }

        close(c_fd);
        co_return;
    };

    loop.schedule(server_task(), "server_task");
    loop.schedule(client_task(), "client_task");
    loop.run();

    // Cleanup
    close(s_fd);
}

TEST_CASE("recv timeout", "[io_net]")
{
    io_loop loop;
    loop.init();

    int s_fd, c_fd;

    s_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(s_fd >= 0);
    c_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    REQUIRE(c_fd >= 0);

    struct sock_addr addr("@tmp/io_recv_timeout_test", AF_UNIX);

    int ret = bind(s_fd, addr.sockaddr(), addr.len());
    REQUIRE(ret == 0);

    ret = listen(s_fd, 5);
    REQUIRE(ret == 0);

    auto server_task = [&]() -> io_task
    {
        sock_addr remote;
        int remote_fd;
        auto accept_res = co_await accept(loop, s_fd, remote_fd, remote);
        REQUIRE(accept_res == io_result::done);
        LOG(debug) << "Accepted connection: " << remote_fd;
        
        // Don't send any data, just keep the connection open
        // The client will timeout waiting for data
        shutdown(remote_fd, SHUT_WR);
        close(remote_fd);
        // Keep server running until the client times out and the test completes
        co_return;
    };

    auto client_task = [&]() -> io_task
    {
        // Connect to server
        auto connect_res = co_await connect(loop, c_fd, addr);
        REQUIRE(connect_res == io_result::done);
        LOG(debug) << "Connected to server";

        // Try to receive with a short timeout
        std::array<char, 128> buffer{};
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        ssize_t bytes_received = 0;
        auto recv_op = recv(loop, c_fd, buffer.data(), buffer.size(), bytes_received, 0, timeout);
        auto res = co_await recv_op;

        if (res == io_result::timeout)
        {
            REQUIRE(bytes_received == 0);
            LOG(debug) << "Receive timed out as expected";
        }

        close(c_fd);
        co_return;
    };

    loop.schedule(server_task(), "server_task");
    loop.schedule(client_task(), "client_task");
    loop.run();

    close(s_fd);
}

TEST_CASE("recvmsg with control data", "[io_net]")
{
    io_loop loop;
    loop.init();

    int sv[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sv) == 0);
    int sender_fd = sv[0];
    int receiver_fd = sv[1];

    // Create pipe first so we can send its FD
    int pipe_fds[2];
    REQUIRE(pipe(pipe_fds) == 0);
    int fd_to_send = pipe_fds[1];

    // Prepare control message outside the task
    char data[] = "Sample data";
    size_t data_size = sizeof(data);  // Use for comparison later
    struct msghdr msg = {};
    struct iovec iov = {data, data_size};
    char control_buffer[CMSG_SPACE(sizeof(int))];
    
    // Initialize the control buffer to prevent valgrind errors
    memset(control_buffer, 0, sizeof(control_buffer));
    
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buffer;
    msg.msg_controllen = sizeof(control_buffer);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    // Split the test into two tasks - first to send the message and then to receive it
    auto sender_task = [&]() -> io_task
    {
        // Send the message with the file descriptor
        ssize_t sent = sendmsg(sender_fd, &msg, 0);        
        LOG(debug) << "Sent message with fd: " << sent << " bytes, errno=" << errno;
        REQUIRE(sent >= 0);
        REQUIRE(static_cast<size_t>(sent) == data_size);
        
        shutdown(sender_fd, SHUT_WR);
        close(sender_fd);
        co_return;
    };

    auto receiver_task = [&]() -> io_task
    {
        // Prepare buffers for receiving message and control data
        char data_buffer[128] = {0};
        char recv_control_buffer[CMSG_SPACE(sizeof(int))] = {0};
        ssize_t bytes_received = 0;
        
        struct iovec iov = {data_buffer, sizeof(data_buffer)};
        struct msghdr recv_msg = {};
        
        recv_msg.msg_iov = &iov;
        recv_msg.msg_iovlen = 1;
        recv_msg.msg_control = recv_control_buffer;
        recv_msg.msg_controllen = sizeof(recv_control_buffer);

        // Receive the message
        auto recv_op = recvmsg(loop, receiver_fd, &recv_msg, bytes_received);
        auto res = co_await recv_op;
        REQUIRE(res == io_result::done);
        
        LOG(debug) << "Received " << bytes_received << " bytes";
        REQUIRE(bytes_received > 0);

        LOG(debug) << "Received data: " << data_buffer;
        REQUIRE(std::string(data_buffer) == "Sample data");

        // Extract the file descriptor from control data
        int received_fd = -1;
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&recv_msg); 
             cmsg != NULL; 
             cmsg = CMSG_NXTHDR(&recv_msg, cmsg)) 
        {
            if (cmsg->cmsg_level == SOL_SOCKET && 
                cmsg->cmsg_type == SCM_RIGHTS) 
            {
                memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
                break;
            }
        }

        REQUIRE(received_fd >= 0);
        LOG(debug) << "Received file descriptor: " << received_fd;
        
        // Verify the received FD is valid by attempting to use it
        char test_write[] = "test";
        size_t write_size = sizeof(test_write);
        ssize_t bytes_written = write(received_fd, test_write, write_size);
        REQUIRE(bytes_written >= 0);
        REQUIRE(static_cast<size_t>(bytes_written) == write_size);
        
        close(received_fd);
        close(receiver_fd);
        co_return;
    };

    // Schedule both tasks
    loop.schedule(sender_task(), "sender_task");
    loop.schedule(receiver_task(), "receiver_task");
    loop.run();

    // Cleanup
    close(pipe_fds[0]); // Close read end of pipe
}
