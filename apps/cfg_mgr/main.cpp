#include <coroutine>
#include <utility>
#include <cassert>
#include <list>
#include <queue>
#include <chrono>
#include <fcntl.h>
#include <thread>
#include <iostream>

#include <iolib/ioloop.hpp>
#include <iolib/dns_resolver.hpp>
#include <iolib/io_ops.hpp>
#include <iolib/net.hpp>
#include <common/uri.hpp>
#include <common/sockaddr.hpp>

#include <common/log.hpp>

using namespace std::chrono_literals;

#if 0
io::io_task<void> test_client(io::io_loop &io)
{
    LOG(info) << "Starting client";
    char buf[1024];
    struct sock_addr in_addr{};
    int sock_fd = -1;

    // let the server start
    co_await io::io_sleep(io, 1s);

    auto &rs = io::dns_resolver::instance(io);

    auto addrs = co_await rs.resolve("localhost", "1234", AF_INET, SOCK_STREAM);

    for (auto &addr : addrs)
    {
        LOG(info) << "Resolved: " << addr.to_string();
        auto s = co_await io::net::sock_connect(io, addr);

        LOG(info).format("Connected: {} fd: {}", to_string(s.first), s.second);

        sock_fd = s.second;
    }

    co_await io::net::sock_sendto(io, sock_fd, "Hello", 5, addrs[0], 10s);
    LOG(debug) << "Wrote data";

    co_await io::io_sleep(io, 20ms);
#if 1
    addrs = co_await rs.resolve("yahoo.com", "80");

    for (auto &addr : addrs)
    {
        LOG(info) << "Resolved: " << addr.to_string();
    }
#endif

    while (true)
    {
        //auto ret = co_await io.io_promise(0, io_event::read, 1s);
        auto ret = co_await io::net::sock_recvfrom(io, sock_fd, buf, sizeof(buf), in_addr, 10s);
        LOG(debug) << "Event: " << to_string(ret.first) << " size: " << ret.second;
        if (ret.first == io::io_result::error || ret.first == io::io_result::shutdown) break;

        if (ret.first == io::io_result::ok)
        {
            LOG(debug) << "Address: " << in_addr.to_string();
        }
    }

    co_return;
}
#endif

io::io_task<io::io_result> handle_ssl_error(io::io_loop &io, SSL *ssl, int ret)
{
    int err = SSL_get_error(ssl, ret);
    int fd  = SSL_get_fd(ssl);

    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    {
        auto ret = co_await io::io_wait_event(io, fd, err == SSL_ERROR_WANT_READ ? io::io_event::read : io::io_event::write, 10s);
        if (ret.first == io::io_result::ok)
        {
            LOG(error) << "Failed to wait for event";
        }
        co_return ret.first;
    }

    LOG(error) << "SSL error: " << err << " " << ERR_error_string(err, nullptr);
    co_return io::io_result::error;
}

io::io_task<void> handle_incoming(io::io_loop &io, SSL *ssl, const sock_addr &addr)
{
    LOG(info) << "Starting client task: " << addr.to_string();
    char buf[1024];
    struct sock_addr in_addr{};

    while (true)
    {
        auto ret = SSL_read(ssl, buf, sizeof(buf));
        if (ret < 0)
        {
            auto r = co_await handle_ssl_error(io, ssl, ret);
            if (r == io::io_result::timeout) { continue; }
            if (r != io::io_result::ok)
            {
                LOG(error) << "Failed to wait for event";
                break;
            }
        }
        else if (ret == 0)
        {
            LOG(info) << "Connection closed";
            break;
        }
        else if (ret > 0)
        {
            LOG(info) << "Read: " << ret << " bytes";
        }
    }
    int fd = SSL_get_fd(ssl);
    io.remove_fd(fd);

    co_return;
}

io::io_task<void> test_server(io::io_loop &io, SSL_CTX *ctx)
{
    LOG(info) << "Starting server";

    //auto &rs = io::dns_resolver::instance(io);

    //auto addrs = co_await rs.resolve("localhost", "1234", AF_UNSPEC, SOCK_STREAM);
    auto addr = sock_addr("127.0.0.1:1234", AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);

    auto [ret, listen_fd] = io::net::sock_listen(io, addr);
    if (ret != io::io_result::ok)
    {
        LOG(error) << "Failed to listen";
        co_return;
    }

    LOG(info) << "Listening on: " << addr.to_string() << " fd: " << listen_fd;

    while (true)
    {
        auto [ret, accept_fd] = co_await io::net::sock_accept(io, listen_fd, addr, 10s);
        if (ret != io::io_result::ok)
        {
            if (ret == io::io_result::timeout) continue;
            LOG(error) << "Failed to accept";
            break;
        }

        LOG(info) << "Accepted connection: " << accept_fd;

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, accept_fd);
        SSL_set_accept_state(ssl);

        int iret = 0;
        do {
            LOG(info) << "Accepting SSL connection";
            iret = SSL_accept(ssl);
            LOG(info) << "SSL_accept returned: " << iret;

            if (iret == 0)
            {
                LOG(error) << "Failed to accept SSL connection: " << SSL_get_error(ssl, iret);
                break;
            }
            else if (iret == 1)
            {
                LOG(info) << "SSL connection accepted";
                io.schedule(std::move(handle_incoming(io, ssl, addr)));
                accept_fd = -1;
                break;
            }
            else
            {
                auto err = co_await handle_ssl_error(io, ssl, iret);
                if (err == io::io_result::ok) { continue; }

                LOG(error) << "Failed to accept SSL connection: " << SSL_get_error(ssl, iret);
                break;
            }
        } while (iret != 1);

        if (accept_fd != -1) close(accept_fd);
    }

    co_return;
}

io::io_task<void> test_time(io::io_loop &io)
{
    LOG(info) << "Starting time test";


    while (true)
    {
        auto p1 = io::io_sleep(io, 1s);
        auto p2 = io::io_sleep(io, 2s);

        co_await io.wait_for({&p1, &p2});
        // LOG(info) << "time: " << to_string(ret.first);
        auto addrs = co_await io::dns_resolve(io, "example.com");
    }

    io.shutdown();
    co_return;
}


int main()
{
    io::io_loop *io_loop = new io::io_loop();

    LOG(info) << "Starting";
    auto key_pair = io::net::generate_self_signed_cert("localhost", 365);
    LOG(info) << "Generated cert";

    SSL_CTX *ctx = io::net::create_ssl_context(*io_loop, key_pair.first, key_pair.second);

    io_loop->schedule(std::move(test_time(*io_loop)));
    //io_loop->schedule(std::move(test_server(*io_loop, ctx)));
    //io_loop->schedule(std::move(test_client(*io_loop)));
    io_loop->run();

    io::net::free_ssl_context(ctx);
    delete io_loop;

    return 0;
}
