#pragma once

#include <io/common.hpp>
#include <net/ops.hpp>
#include <net/sockaddr.hpp>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <coroutine>

namespace io
{

// Add a utility class for socket configuration
struct socket_config
{
    bool keep_alive         = false;
    int keep_alive_idle     = 60;
    int keep_alive_interval = 10;
    int keep_alive_count    = 3;
    bool tcp_nodelay        = true;
    int send_buffer_size    = 0;
    int recv_buffer_size    = 0;

    void apply(int fd) const
    {
        if (keep_alive)
        {
            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count));
        }
        if (tcp_nodelay)
        {
            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        }
        if (send_buffer_size > 0)
        {
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size));
        }
        if (recv_buffer_size > 0)
        {
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size));
        }
    }
};

namespace detail
{

inline void handle_socket_error(std::error_code &error, const char *operation)
{
    error = std::error_code(errno, std::system_category());
    LOG(error) << "Failed to " << operation << ": " << error.message();
}

struct io_accept : public io_op_base
{
    struct sock_addr &remote_;
    int &remote_fd_;
    bool completed_ = false;

    io_accept()                  = delete;
    io_accept(const io_accept &) = delete;

    io_accept(io_loop &loop, int fd, int &remote_fd, struct sock_addr &remote, time_point_t complete_by = time_point_t::max()) noexcept
    : io_op_base{loop, fd, io_desc_type::read, complete_by},
      remote_{remote},
      remote_fd_{remote_fd}
    {
    }

    bool check_ready() noexcept override
    {
        // Let the I/O loop's poller handle readiness detection
        // We just need to tell if we're willing to progress
        execute();
        return completed_;
    }

    void execute() noexcept override
    {
        if (completed_) return;

        // Accept the connection
        int new_fd = accept4(waiter_.fd(), reinterpret_cast<struct sockaddr *>(remote_.sockaddr()), &remote_.len_ref(), SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (new_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Not ready yet, don't complete the waiter
                return;
            }
            else
            {
                handle_socket_error(error_, "accept");
                waiter_.complete(io_result::error);
            }
        }
        else
        {
            remote_fd_ = new_fd;
            completed_ = true;
            waiter_.complete(io_result::done);
        }
    }
};

struct io_connect : public io_op_base
{
    io_connect()                   = delete;
    io_connect(const io_connect &) = delete;

    io_connect(io_loop &loop, int fd, const struct sock_addr &remote, time_point_t complete_by = time_point_t::max()) noexcept
    : io_op_base{loop, fd, io_desc_type::write, complete_by},
      remote_{remote}
    {
    }

    bool check_ready() noexcept override
    {
        execute();
        // We're ready if not in progress
        return !in_progress_;
    }

    void execute() noexcept override
    {
        if (in_progress_)
        {
            // Check if connection completed
            int error     = 0;
            socklen_t len = sizeof(error);
            int ret       = getsockopt(waiter_.fd(), SOL_SOCKET, SO_ERROR, &error, &len);

            if (ret == -1)
            {
                handle_socket_error(error_, "getsockopt");
                waiter_.complete(io_result::error);
            }
            else if (error != 0)
            {
                error_ = std::error_code(error, std::system_category());
                LOG(error) << "Failed to connect: " << error_.message();
                waiter_.complete(io_result::error);
            }
            else
            {
                LOG(debug) << "Connected to: " << remote_.to_string();
                waiter_.complete(io_result::done);
            }
        }
        else
        {
            // First time - try to connect
            int ret = ::connect(waiter_.fd(), remote_.sockaddr(), remote_.len());

            if (ret == -1)
            {
                if (errno == EINPROGRESS)
                {
                    // Mark as in progress for next time
                    in_progress_ = true;
                    // Don't complete the waiter yet
                }
                else if (errno == EISCONN)
                {
                    // Socket is already connected, consider this a success
                    LOG(debug) << "Socket already connected to: " << remote_.to_string();
                    waiter_.complete(io_result::done);
                }
                else
                {
                    handle_socket_error(error_, "connect");
                    waiter_.complete(io_result::error);
                }
            }
            else
            {
                // Connected immediately
                LOG(debug) << "Connected to: " << remote_.to_string();
                waiter_.complete(io_result::done);
            }
        }
    }

  private:
    struct sock_addr remote_;
    bool in_progress_ = false;
};

struct io_recv : public io_op_base
{
    void *buffer_;
    size_t buffer_size_;
    int flags_;
    ssize_t &bytes_received_;
    bool closed_         = false;
    bool read_completed_ = false;

    io_recv()                = delete;
    io_recv(const io_recv &) = delete;

    io_recv(io_loop &loop,
            int fd,
            void *buffer,
            size_t buffer_size,
            ssize_t &bytes_received,
            int flags                = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
    : io_op_base{loop, fd, io_desc_type::read, complete_by},
      buffer_{buffer},
      buffer_size_{buffer_size},
      flags_{flags},
      bytes_received_{bytes_received}
    {
        bytes_received_ = 0;
    }

    bool check_ready() noexcept override
    {
        if (!read_completed_)
        {
            execute(); // Only execute if we haven't completed a read
        }
        return closed_ || bytes_received_ > 0;
    }

    void execute() noexcept override
    {
        // Skip redundant execution if we've already read data
        if (read_completed_) return;

        // Perform the actual read
        ssize_t result = ::recv(waiter_.fd(), buffer_, buffer_size_, flags_);

        if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Not ready yet
                return;
            }
            else
            {
                handle_socket_error(error_, "recv");
                waiter_.complete(io_result::error);
            }
        }
        else if (result == 0)
        {
            // Connection closed
            bytes_received_ = 0;
            closed_         = true;
            waiter_.complete(io_result::closed);
        }
        else
        {
            bytes_received_ = result;
            read_completed_ = true; // Mark read as completed
            LOG(trace) << "Received " << bytes_received_ << " bytes";
            waiter_.complete(io_result::done);
        }
    }
};

struct io_recvmsg : public io_op_base
{
    struct msghdr *msg_;
    int flags_;
    ssize_t &bytes_received_;
    bool closed_         = false;
    bool read_completed_ = false; // Add the same flag here for consistency

    io_recvmsg()                   = delete;
    io_recvmsg(const io_recvmsg &) = delete;

    io_recvmsg(io_loop &loop, int fd, struct msghdr *msg, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
    : io_op_base{loop, fd, io_desc_type::read, complete_by},
      msg_{msg},
      flags_{flags},
      bytes_received_{bytes_received}
    {
        bytes_received_ = 0;
    }

    bool check_ready() noexcept override
    {
        if (!read_completed_)
        {
            execute(); // Only execute if we haven't completed a read
        }
        return closed_ || bytes_received_ > 0;
    }

    void execute() noexcept override
    {
        // Skip redundant execution if we've already read data
        if (read_completed_) return;

        // Perform the actual recvmsg operation
        ssize_t result = ::recvmsg(waiter_.fd(), msg_, flags_);

        if (result == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Not ready yet, don't complete the waiter
                return;
            }
            else
            {
                handle_socket_error(error_, "recvmsg");
                waiter_.complete(io_result::error);
            }
        }
        else if (result == 0)
        {
            // Connection closed
            bytes_received_ = 0;
            closed_         = true;
            waiter_.complete(io_result::closed);
        }
        else
        {
            bytes_received_ = result;
            read_completed_ = true; // Mark read as completed
            LOG(trace) << "Received " << bytes_received_ << " bytes via recvmsg";
            waiter_.complete(io_result::done);
        }
    }
};

struct io_send : public io_op_base
{
    const char *buffer_;
    size_t buffer_size_;
    int flags_;
    size_t &bytes_sent_;

    io_send()                = delete;
    io_send(const io_send &) = delete;

    io_send(io_loop &loop,
            int fd,
            const char *buffer,
            size_t buffer_size,
            size_t &bytes_sent,
            int flags                = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
    : io_op_base{loop, fd, io_desc_type::write, complete_by},
      buffer_{buffer},
      buffer_size_{buffer_size},
      flags_{flags},
      bytes_sent_{bytes_sent}
    {
        bytes_sent_ = 0;
    }

    bool check_ready() noexcept override
    {
        execute();
        // if there is a timeout then we are done only if bytes_sent_ == buffer_size_ or if timeout is reached
        // without a timeout we are done if bytes_sent_ > 0 or if the buffer is empty
        return waiter_.result() == io_result::timeout ? (bytes_sent_ == buffer_size_)
                                                      : ((bytes_sent_ > 0) || (buffer_size_ == 0));
    }

    void execute() noexcept override
    {
        // Perform the actual send
        auto ret = ::send(waiter_.fd(), buffer_ + bytes_sent_, buffer_size_ - bytes_sent_, flags_);

        if (ret == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Not ready yet
                return;
            }
            else
            {
                handle_socket_error(error_, "send");
                waiter_.complete(io_result::error);
            }
        }
        else
        {
            waiter_.complete(io_result::done);
            bytes_sent_ += ret;
            LOG(trace) << "Sent " << bytes_sent_ << " bytes";
        }
    }
};

} // namespace detail

// Add the new overload with socket configuration
auto connect(io_loop &loop,
             int fd,
             const struct sock_addr &remote,
             const socket_config &config = {},
             time_point_t complete_by    = time_point_t::max()) noexcept
{
    if (fd >= 0) { config.apply(fd); }
    return detail::io_connect{loop, fd, remote, complete_by};
}

auto accept(io_loop &loop, int fd, int &remote_fd, struct sock_addr &remote, time_point_t complete_by = time_point_t::max()) noexcept
{
    return detail::io_accept{loop, fd, remote_fd, remote, complete_by};
}

auto recv(io_loop &loop, int fd, char *buffer, size_t buffer_size, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    return detail::io_recv{loop, fd, buffer, buffer_size, bytes_received, flags, complete_by};
}

auto recvmsg(io_loop &loop, int fd, struct msghdr *msg, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    return detail::io_recvmsg{loop, fd, msg, bytes_received, flags, complete_by};
}

auto send(io_loop &loop, int fd, const char *buffer, size_t buffer_size, size_t &bytes_sent, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    return detail::io_send{loop, fd, buffer, buffer_size, bytes_sent, flags, complete_by};
}

} // namespace io
