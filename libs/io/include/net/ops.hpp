#pragma once

#include <io/common.hpp>

// Remove the circular include
#include <net/sockaddr.hpp>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <coroutine>

#ifdef DEBUG
#include <cassert>
#endif

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

    // Settings to apply before connecting
    void apply_pre_connect(int fd) const
    {
        if (send_buffer_size > 0)
        {
            if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) < 0) {
                LOG(error) << "Failed to set SO_SNDBUF: " << std::error_code(errno, std::system_category()).message();
            }
        }
        if (recv_buffer_size > 0)
        {
            if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) < 0) {
                LOG(error) << "Failed to set SO_RCVBUF: " << std::error_code(errno, std::system_category()).message();
            }
        }
    }

    // Settings to apply after connection is established
    void apply_post_connect(int fd) const
    {
        if (keep_alive)
        {
            int opt = 1;
            // Add error checking for setsockopt calls
            if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
                LOG(error) << "Failed to set SO_KEEPALIVE: " << std::error_code(errno, std::system_category()).message();
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle)) < 0) {
                LOG(error) << "Failed to set TCP_KEEPIDLE: " << std::error_code(errno, std::system_category()).message();
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval)) < 0) {
                LOG(error) << "Failed to set TCP_KEEPINTVL: " << std::error_code(errno, std::system_category()).message();
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_count, sizeof(keep_alive_count)) < 0) {
                LOG(error) << "Failed to set TCP_KEEPCNT: " << std::error_code(errno, std::system_category()).message();
            }
        }
        if (tcp_nodelay)
        {
            int opt = 1;
            if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
                LOG(error) << "Failed to set TCP_NODELAY: " << std::error_code(errno, std::system_category()).message();
            }
        }
    }
};

namespace detail
{

struct io_accept : public io_desc_awaitable
{
    struct sock_addr &remote_;
    int &remote_fd_;
    bool completed_ = false;

    io_accept()                  = delete;
    io_accept(const io_accept &) = delete;

    io_accept(io_loop &loop, int fd, int &remote_fd, struct sock_addr &remote, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::read, complete_by},
      remote_{remote},
      remote_fd_{remote_fd}
    {
    }

    bool check_ready() noexcept override
    {
        // Let the I/O loop's poller handle readiness detection
        // We just need to tell if we're willing to progress
        execute();
        return completed_ || has_error();
    }

    void execute() noexcept
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
                set_error(std::error_code(errno, std::system_category()));
                LOG(error) << "Failed to accept: " << error_.message();
                completed_ = true;
            }
        }
        else
        {
            remote_fd_ = new_fd;
            completed_ = true;
        }
    }
};

struct io_connect : public io_desc_awaitable
{
    io_connect()                   = delete;
    io_connect(const io_connect &) = delete;

    io_connect(io_loop &loop, int fd, const struct sock_addr &remote, const socket_config &config, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::write, complete_by},
      remote_{remote},
      config_{config}
    {
    }

    bool check_ready() noexcept override
    {
        execute();
        // We're ready if not in progress
        return !in_progress_ || has_error();
    }

    void execute() noexcept
    {
        if (in_progress_)
        {
            // Check if connection completed
            int error     = 0;
            socklen_t len = sizeof(error);
            int ret       = getsockopt(waiter_.fd(), SOL_SOCKET, SO_ERROR, &error, &len);

            if (ret == -1)
            {
                set_error(std::error_code(errno, std::system_category()));
                LOG(error) << "Failed to connect: " << error_.message();
            }
            else if (error != 0)
            {
                set_error(std::error_code(error, std::system_category()));
                LOG(error) << "Failed to connect: " << error_.message();
            }
            else
            {
                LOG(debug) << "Connected to: " << remote_.to_string();
                in_progress_ = false;
                
                // Apply post-connect settings
                config_.apply_post_connect(waiter_.fd());
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
                    in_progress_ = false;
                    
                    // Apply post-connect settings
                    config_.apply_post_connect(waiter_.fd());
                }
                else
                {
                    set_error(std::error_code(errno, std::system_category()));
                    LOG(error) << "Failed to connect: " << error_.message();
                }
            }
            else
            {
                // Connected immediately
                LOG(debug) << "Connected to: " << remote_.to_string();
                
                // Apply post-connect settings
                config_.apply_post_connect(waiter_.fd());
            }
        }
    }

  private:
    struct sock_addr remote_;
    bool in_progress_ = false;
    const socket_config &config_;
};

struct io_recvfrom : public io_desc_awaitable
{
    void *buffer_;
    size_t buffer_size_;
    int flags_;
    ssize_t &bytes_received_;
    bool closed_;
    struct sockaddr *src_addr_;
    socklen_t *addrlen_;

    io_recvfrom() = delete;
    io_recvfrom(const io_recvfrom &) = delete;

    io_recvfrom(io_loop &loop,
            int fd,
            void *buffer,
            size_t buffer_size,
            ssize_t &bytes_received,
            struct sockaddr *src_addr = nullptr,
            socklen_t *addrlen = nullptr,
            int flags = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::read, complete_by},
      buffer_{buffer},
      buffer_size_{buffer_size},
      flags_{flags},
      bytes_received_{bytes_received},
      closed_{false},
      src_addr_{src_addr},
      addrlen_{addrlen}
    {
        bytes_received_ = 0;
    }

    bool check_ready() noexcept override
    {
        LOG(trace) << "fd: " << waiter_.fd() << " ready type: " << to_string(waiter_.type());
        execute();
        return has_error() || closed_ || bytes_received_ >= static_cast<ssize_t>(buffer_size_);
    }

    bool check_closed() noexcept override
    {
        return closed_;
    }

    void execute() noexcept
    {
        // Perform the actual read
        while (bytes_received_ < static_cast<ssize_t>(buffer_size_))
        {
            ssize_t result;
            
            if (src_addr_ != nullptr && addrlen_ != nullptr) {
                result = ::recvfrom(waiter_.fd(), 
                                    static_cast<char *>(buffer_) + bytes_received_, 
                                    buffer_size_ - bytes_received_, 
                                    flags_,
                                    src_addr_,
                                    addrlen_);
            } else {
                // Fall back to regular recv if no source address buffer provided
                result = ::recv(waiter_.fd(), 
                                static_cast<char *>(buffer_) + bytes_received_, 
                                buffer_size_ - bytes_received_, 
                                flags_);
            }

            LOG(trace) << "fd: " << waiter_.fd() << " recvfrom result: " << result << " bytes_received: " << bytes_received_ << " buffer_size: " << buffer_size_;

            if (result == -1)
            {
                LOG(trace) << "fd: " << waiter_.fd() << " recvfrom error: " << errno << " " << std::error_code(errno, std::system_category()).message();
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    LOG(trace) << "fd: " << waiter_.fd() << " Not ready yet";
                    break;
                }
                else
                {
                    set_error(std::error_code(errno, std::system_category()));
                    break;
                }
            }
            else if (result == 0)
            {
                // Connection closed
                LOG(trace) << "fd: " << waiter_.fd() << " Connection closed";
                closed_ = true;
                break;
            }
            else
            {
                LOG(trace) << "fd: " << waiter_.fd() << " Received " << result << " bytes, buffer size: " << buffer_size_;
                bytes_received_ += result;

                // If we've read everything requested, mark as done
                if (bytes_received_ == static_cast<ssize_t>(buffer_size_))
                {
                    LOG(trace) << "Received all " << bytes_received_ << " bytes requested";
                    break;
                }
                
                // For connectionless sockets like UDP, we only get one datagram per call
                // so we don't continue reading if we received anything and are using src_addr
                if (src_addr_ != nullptr && bytes_received_ > 0) {
                    LOG(trace) << "Received datagram with source address, stopping";
                    break;
                }
                // else need more data for connection-oriented sockets
            }
        }
    }
};

struct io_recvmsg : public io_desc_awaitable
{
    struct msghdr *msg_;
    void *msg_control_;
    size_t msg_control_len_;

    int flags_;
    ssize_t &bytes_received_;
    ssize_t total_capacity_; // Total capacity across all iovecs
    bool closed_;
    bool first_call_;

    io_recvmsg()                   = delete;
    io_recvmsg(const io_recvmsg &) = delete;

    io_recvmsg(io_loop &loop, int fd, struct msghdr *msg, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::read, complete_by},
      msg_{msg},
      flags_{flags},
      bytes_received_{bytes_received},
      total_capacity_{0},
      closed_{false},
      first_call_{true}
    {
        bytes_received_ = 0;
        
        // Calculate total capacity across all iovecs
        for (size_t i = 0; i < msg_->msg_iovlen; ++i) {
            total_capacity_ += msg_->msg_iov[i].iov_len;
        }
    }

    bool check_ready() noexcept override
    {
        execute();
        return has_error() || closed_ || bytes_received_ == total_capacity_;
    }

    bool check_closed() noexcept override
    {
        return closed_;
    }

    void completed() noexcept override
    {
        if (msg_control_ != nullptr && !first_call_)
        {
            // restore the control data for the caller
            msg_->msg_control    = msg_control_;
            msg_->msg_controllen = msg_control_len_;

            msg_control_     = nullptr;
            msg_control_len_ = 0;
        }
    }

    void execute() noexcept
    {
        while (bytes_received_ < total_capacity_)
        {
            // Perform the actual recvmsg operation
            ssize_t result = ::recvmsg(waiter_.fd(), msg_, flags_);

            if (result == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Not ready yet, will be retried by the polling loop
                    break;
                }
                else
                {
                    set_error(std::error_code(errno, std::system_category()));
                    if (errno == ECONNRESET) { closed_ = true; }
                    break;
                }
            }
            else if (result == 0)
            {
                set_error(std::error_code(ECONNRESET, std::system_category()));
                closed_ = true;
                break;
            }
            else
            {
                if (first_call_)
                {
                    LOG(trace) << "Received " << result << " bytes via recvmsg";
                    first_call_ = false;
                    // squirrel away the control data for later
                    msg_control_ = msg_->msg_control;
                    msg_control_len_ = msg_->msg_controllen;
                }

                // Update our tracking of how much we've received
                bytes_received_ += result;

                if (bytes_received_ == total_capacity_)
                {
                    LOG(trace) << "Received all " << bytes_received_ << " bytes via recvmsg";
                    break;
                }

                // Adjust iovec structures for any subsequent read
                size_t bytes_handled = result;

                for (size_t i = 0; i < msg_->msg_iovlen; ++i)
                {
                    if (bytes_handled <= msg_->msg_iov[i].iov_len)
                    {
                        // This iovec wasn't fully consumed
                        msg_->msg_iov[i].iov_base = static_cast<char *>(msg_->msg_iov[i].iov_base) + bytes_handled;
                        msg_->msg_iov[i].iov_len -= bytes_handled;
                        break;
                    }
                    else
                    {
                        // This iovec was fully consumed, move to next
                        bytes_handled -= msg_->msg_iov[i].iov_len;
                        msg_->msg_iov[i].iov_len = 0;
                    }
                }
            }
        }
    }
};

struct io_sendto : public io_desc_awaitable
{
    const char *buffer_;
    size_t buffer_size_;
    int flags_;
    size_t &bytes_sent_;
    const struct sockaddr *dest_addr_;
    socklen_t addrlen_;

    io_sendto()                 = delete;
    io_sendto(const io_sendto &) = delete;

    io_sendto(io_loop &loop,
            int fd,
            const char *buffer,
            size_t buffer_size,
            size_t &bytes_sent,
            const struct sockaddr *dest_addr = nullptr,
            socklen_t addrlen = 0,
            int flags                = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::write, complete_by},
      buffer_{buffer},
      buffer_size_{buffer_size},
      flags_{flags},
      bytes_sent_{bytes_sent},
      dest_addr_{dest_addr},
      addrlen_{addrlen}
    {
        bytes_sent_ = 0;
    }

    bool check_ready() noexcept override
    {
        execute();
        return has_error() || bytes_sent_ == buffer_size_;
    }

    void execute() noexcept
    {
        while(bytes_sent_ < buffer_size_)
        {
            // Perform the actual sendto
            ssize_t ret;
            
            if (dest_addr_ != nullptr) {
                ret = ::sendto(waiter_.fd(), buffer_ + bytes_sent_, buffer_size_ - bytes_sent_, 
                               flags_, dest_addr_, addrlen_);
            } else {
                // If no dest_addr is provided, use send instead (equivalent behavior)
                ret = ::send(waiter_.fd(), buffer_ + bytes_sent_, buffer_size_ - bytes_sent_, flags_);
            }

            LOG(trace) << "::sendto() returned " << ret << " errnos: " << errno << " " << strerror(errno);

            if (ret == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    LOG(trace) << "Not ready yet, will be retried by the polling loop";
                    break;
                }
                else
                {
                    set_error(std::error_code(errno, std::system_category()));
                    break;
                }
            }
            else
            {
                bytes_sent_ += ret;

                LOG(trace) << "bytes_sent_ = " << bytes_sent_ << " buffer_size_ = " << buffer_size_;

                if (bytes_sent_ == buffer_size_) { LOG(trace) << "Sent all " << bytes_sent_ << " bytes"; }
            }
        }
    }
};

struct io_sendmsg : public io_desc_awaitable
{
    struct msghdr *msg_;
    int flags_;
    size_t &bytes_sent_;
    size_t full_size_;
    // Add variables to store control message data
    void *msg_control_;
    size_t msg_control_len_;
    bool first_call_;

    io_sendmsg()                   = delete;
    io_sendmsg(const io_sendmsg &) = delete;

    io_sendmsg(io_loop &loop, int fd, struct msghdr *msg, size_t &bytes_sent, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_awaitable{loop, fd, io_desc_type::write, complete_by},
      msg_{msg},
      flags_{flags},
      bytes_sent_{bytes_sent},
      full_size_{0},
      msg_control_{nullptr},
      msg_control_len_{0},
      first_call_{true}
    {
        bytes_sent_ = 0;

        // Calculate the full size of the message
        for (size_t i = 0; i < msg_->msg_iovlen; ++i)
        {
            full_size_ += msg_->msg_iov[i].iov_len;
        }
    }

    bool check_ready() noexcept override
    {
        execute();
        return has_error() || bytes_sent_ == full_size_;
    }

    void completed() noexcept override
    {
        if (msg_control_ != nullptr && !first_call_)
        {
            // Restore the original control message data if we stored it
            msg_->msg_control = msg_control_;
            msg_->msg_controllen = msg_control_len_;
            
            msg_control_ = nullptr;
            msg_control_len_ = 0;
        }
    }

    void execute() noexcept
    {
        while (bytes_sent_ < full_size_)
        {
            // For the first call, save the control message data
            if (first_call_ && msg_->msg_control != nullptr)
            {
                msg_control_ = msg_->msg_control;
                msg_control_len_ = msg_->msg_controllen;
                first_call_ = false;
                
                LOG(trace) << "Stored control message data: " << msg_control_ << ", len: " << msg_control_len_;
            }

            // Perform the actual sendmsg
            auto ret = ::sendmsg(waiter_.fd(), msg_, flags_);

            LOG(trace) << "::sendmsg() returned " << ret << " errno: " << errno << " " << strerror(errno);

            if (ret == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    LOG(trace) << "Not ready yet, will be retried by the polling loop";
                    break;
                }
                else
                {
                    set_error(std::error_code(errno, std::system_category()));
                    break;
                }
            }
            else
            {
                // Update total bytes sent
                bytes_sent_ += ret;
                
                LOG(trace) << "bytes_sent_ = " << bytes_sent_ << " full_size_ = " << full_size_;
                
                if (bytes_sent_ == full_size_)
                {
                    LOG(trace) << "Sent all " << bytes_sent_ << " bytes via sendmsg";
                    break;
                }

                // After the first send, clear the control data since it's already been sent
                // Control data is only sent on the first sendmsg call
                if (!first_call_ && msg_->msg_control != nullptr)
                {
                    msg_->msg_control = nullptr;
                    msg_->msg_controllen = 0;
                }

                // Adjust iovec structures for the next send
                size_t bytes_handled = ret;
                for (size_t i = 0; i < msg_->msg_iovlen; ++i)
                {
                    if (bytes_handled >= msg_->msg_iov[i].iov_len)
                    {
                        // This iovec was fully consumed
                        bytes_handled -= msg_->msg_iov[i].iov_len;
                        msg_->msg_iov[i].iov_len = 0;
                    }
                    else
                    {
                        // This iovec was partially consumed
                        msg_->msg_iov[i].iov_base = static_cast<char *>(msg_->msg_iov[i].iov_base) + bytes_handled;
                        msg_->msg_iov[i].iov_len -= bytes_handled;
                        break;
                    }
                }
            }
        }
    }
};

} // namespace detail

/**
 * @defgroup io_net Asynchronous network I/O operations
 * @brief Asynchronous network I/O operations
 *
 * These functions provide a way to perform network I/O operations asynchronously.
 *
 * All of the defined here follow the same pattern:
 * - They take an IO loop to use
 * - They take a file descriptor to operate on
 * - And any additional parameters needed for the operation
 * - They return an awaitable object that can be used in a coroutine
 * - The awaitable object returns an io_result when awaited
 * - io_result::done indicates that the operation completed successfully before timeout expired
 *   or more specifically, even if the timeout is set the operation will have one more chance to complete.
 * - The operations will never discard any data it receives, even after an error or timeout (or the like) for operations
 *   like recv and recvmsg the partial data will be stored in the provided buffer.
 *
 * @note These operations are not thread-safe. A single operation should not be used concurrently from multiple threads.
 * 
 * @{
 */

/**
 * Asynchronously connect to a remote address
 * 
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param remote Remote address to connect to
 * @param config Socket configuration to apply
 * @param complete_by Optional timeout for the operation
 * 
 * @return An awaitable connect operation that returns the following when awaited:
 *         - io_result::done Connection established before timeout (or immediately if already connected)
 *         - io_result::error on connection error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto connect(io_loop &loop,
             int fd,
             const struct sock_addr &remote,
             const socket_config &config = {},
             time_point_t complete_by    = time_point_t::max()) noexcept
{
    if (fd >= 0) { config.apply_pre_connect(fd); }
    return detail::io_connect{loop, fd, remote, config, complete_by};
}

/**
 * Asynchronously accept a connection
 * 
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param remote_fd Reference to store the accepted socket file descriptor
 * @param remote Remote address to accept from
 * @param complete_by Optional timeout for the operation
 * 
 * @return An awaitable accept operation that returns the following when awaited:
 *         - io_result::done when a connection is accepted (remote_fd and remote are filled)
 *         - io_result::error on accept error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto accept(io_loop &loop, int fd, int &remote_fd, struct sock_addr &remote, time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "accept: fd is invalid");
    #endif

    return detail::io_accept{loop, fd, remote_fd, remote, complete_by};
}

/**
 * Asynchronously receive data.
 *
 * With @e complete_by set, read will wait until the specified time for the @p buffer to be fully filled or if @p fd has
 * an error or is closed. If @e complete_by is not set, the operation will wait indefinitely for the buffer to be fully.
 *
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer to store received data. The buffer will contain as much data as the operation was able to read
 *               before timing out or encountering an error. @note The memory must be valid until the operation completes.
 * @param buffer_size Size of the buffer.
 * @param [out] bytes_received Reference to store the number of bytes received. Can be set from a partial read.
 * @param flags Optional flags for the recv operation
 * @param complete_by Optional timeout for the operation. Will wait indefinitely if not set.
 *
 * @return An awaitable recv operation that returns the following when awaited:
 *         - io_result::done when data is successfully received (bytes_received will contain the amount)
 *         - io_result::closed when the peer closed the connection
 *         - io_result::error on receive error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto recv(io_loop &loop, int fd, char *buffer, size_t buffer_size, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "recv: fd is invalid");
    assert(buffer != nullptr && "recv: buffer is nullptr");
    #endif

    return detail::io_recvfrom{loop, fd, buffer, buffer_size, bytes_received, nullptr, nullptr, flags, complete_by};
}

/**
 * Asynchronously receive data from a specific source address
 *
 * With @e complete_by set, read will wait until the specified time for the @p buffer to be fully filled or if @p fd has
 * an error or is closed. If @e complete_by is not set, the operation will wait indefinitely for the buffer to be fully
 * filled.
 *
 * For connectionless protocols like UDP, this function will return after receiving a single datagram, even if
 * the datagram is smaller than buffer_size. For connection-oriented protocols like TCP, it behaves like recv().
 *
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @param [out] bytes_received Reference to store the number of bytes received
 * @param [out] src_addr Pointer to storage for the source address
 * @param [in,out] addrlen Pointer to size of source address structure (modified on return to reflect actual size)
 * @param flags Optional flags for the recvfrom operation
 * @param complete_by Optional timeout for the operation
 *
 * @return An awaitable recvfrom operation that returns the following when awaited:
 *         - io_result::done when data is successfully received (bytes_received will contain the amount)
 *         - io_result::closed when the peer closed the connection
 *         - io_result::error on receive error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto recvfrom(io_loop &loop, int fd, char *buffer, size_t buffer_size, ssize_t &bytes_received,
             struct sockaddr *src_addr, socklen_t *addrlen, int flags = 0,
             time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "recvfrom: fd is invalid");
    assert(buffer != nullptr && "recvfrom: buffer is nullptr");
    assert(src_addr != nullptr && "recvfrom: src_addr is nullptr");
    assert(addrlen != nullptr && "recvfrom: addrlen is nullptr");
    #endif
    return detail::io_recvfrom{loop, fd, buffer, buffer_size, bytes_received, src_addr, addrlen, flags, complete_by};
}

/**
 * Asynchronously receive data from a specific source address
 *
 * Convenience overload that takes a sock_addr object.
 *
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @param [out] bytes_received Reference to store the number of bytes received
 * @param [out] src_addr Reference to sock_addr object that will store the source address
 * @param flags Optional flags for the recvfrom operation
 * @param complete_by Optional timeout for the operation
 *
 * @return An awaitable recvfrom operation that returns the following when awaited:
 *         - io_result::done when data is successfully received (bytes_received will contain the amount)
 *         - io_result::closed when the peer closed the connection
 *         - io_result::error on receive error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto recvfrom(io_loop &loop, int fd, char *buffer, size_t buffer_size, ssize_t &bytes_received,
             struct sock_addr &src_addr, int flags = 0,
             time_point_t complete_by = time_point_t::max()) noexcept
{
    return recvfrom(loop, fd, buffer, buffer_size, bytes_received, 
                   reinterpret_cast<struct sockaddr*>(src_addr.sockaddr()), &src_addr.len_ref(), 
                   flags, complete_by);
}

/**
 * Asynchronously receive a message with ancillary data
 *
 * With @e complete_by set, read will wait until the specified time for the @p msg to be fully filled or if @p fd has an
 * error or is closed. If @e complete_by is not set, the operation will wait indefinitely for the message to be fully
 * received.
 *
 * Control message handling:
 * - The msg_control pointer in msghdr is saved during first call
 * - After operation completion, the original pointer is restored
 * - The caller retains ownership of msg_control memory
 *
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param msg Message header to store received data. The message will contain as much data as the operation was able to
 *            read before timing out or encountering an error. @note The memory must remain valid until the operation
 *            completes.
 * @param [out] bytes_received Reference to store the number of bytes received
 * @param flags Optional flags for the recvmsg operation
 * @param complete_by Optional timeout for the operation. Will wait indefinitely if not set.
 *
 * @return An awaitable recvmsg operation that returns the following when awaited:
 *         - io_result::done when data is successfully received (bytes_received will contain the amount)
 *         - io_result::closed when the peer closed the connection
 *         - io_result::error on receive error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto recvmsg(io_loop &loop, int fd, struct msghdr *msg, ssize_t &bytes_received, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "recvmsg: fd is invalid");
    assert(msg != nullptr && "recvmsg: msg is nullptr");
    #endif
    return detail::io_recvmsg{loop, fd, msg, bytes_received, flags, complete_by};
}

/**
 * Asynchronously send data
 * 
 * The function follows same semantics as the send system call. It waits for the @p fd to be ready for writing and then
 * sends as much data as possible from the buffer in a single call.
 * 
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer containing data to send. 
 * @param buffer_size Size of the buffer
 * @param bytes_sent Reference to store the number of bytes sent
 * @param flags Optional flags for the send operation
 * @param complete_by Optional timeout for the operation
 * 
 * @return An awaitable send operation that returns the following when awaited:
 *         - io_result::done when data is successfully sent (bytes_sent will contain the amount)
 *         - io_result::error on send error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto send(io_loop &loop, int fd, const char *buffer, size_t buffer_size, size_t &bytes_sent, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "send: fd is invalid");
    assert(buffer != nullptr && "send: buffer is nullptr");
    #endif
    return detail::io_sendto{loop, fd, buffer, buffer_size, bytes_sent, nullptr, 0, flags, complete_by};
}

/**
 * Asynchronously send data to a specific destination address
 *
 * The function follows same semantics as the sendto system call. It waits for the @p fd to be ready for writing and
 * then sends as much data as possible from the buffer to the specified destination in a single call.
 *
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer containing data to send
 * @param buffer_size Size of the buffer
 * @param bytes_sent Reference to store the number of bytes sent
 * @param dest_addr Destination address
 * @param addrlen Length of the destination address
 * @param flags Optional flags for the sendto operation
 * @param complete_by Optional timeout for the operation
 *
 * @return An awaitable sendto operation that returns the following when awaited:
 *         - io_result::done when data is successfully sent (bytes_sent will contain the amount)
 *         - io_result::error on send error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto sendto(io_loop &loop,
            int fd,
            const char *buffer,
            size_t buffer_size,
            size_t &bytes_sent,
            const struct sockaddr *dest_addr,
            socklen_t addrlen,
            int flags                = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "sendto: fd is invalid");
    assert(buffer != nullptr && "sendto: buffer is nullptr");
    assert(dest_addr != nullptr && "sendto: dest_addr is nullptr");
    #endif
    return detail::io_sendto{loop, fd, buffer, buffer_size, bytes_sent, dest_addr, addrlen, flags, complete_by};
}

/**
 * Asynchronously send data to a specific destination address
 * 
 * Convenience overload that takes a sock_addr object.
 * 
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param buffer Buffer containing data to send
 * @param buffer_size Size of the buffer
 * @param bytes_sent Reference to store the number of bytes sent
 * @param dest_addr Destination address
 * @param flags Optional flags for the sendto operation
 * @param complete_by Optional timeout for the operation
 * 
 * @return An awaitable sendto operation that returns the following when awaited:
 *         - io_result::done when data is successfully sent (bytes_sent will contain the amount)
 *         - io_result::error on send error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto sendto(io_loop &loop,
            int fd,
            const char *buffer,
            size_t buffer_size,
            size_t &bytes_sent,
            const struct sock_addr &dest_addr,
            int flags                = 0,
            time_point_t complete_by = time_point_t::max()) noexcept
{
    return sendto(loop, fd, buffer, buffer_size, bytes_sent, dest_addr.sockaddr(), dest_addr.len(), flags, complete_by);
}

/**
 * Asynchronously send a message with ancillary data
 * 
 * The function follows same semantics as the sendmsg system call. It waits for the @p fd to be ready for writing and then
 * sends as much data as possible from the message header in a single call.
 * 
 * @param loop The IO loop to use
 * @param fd Socket file descriptor
 * @param msg Message header containing data to send
 * @param bytes_sent Reference to store the number of bytes sent
 * @param flags Optional flags for the sendmsg operation
 * @param complete_by Optional timeout for the operation
 * 
 * @return An awaitable sendmsg operation that returns the following when awaited:
 *         - io_result::done when data is successfully sent (bytes_sent will contain the amount)
 *         - io_result::error on send error (use std::error_code() to get details)
 *         - io_result::timeout if operation timed out
 */
auto sendmsg(io_loop &loop, int fd, struct msghdr *msg, size_t &bytes_sent, int flags = 0, time_point_t complete_by = time_point_t::max()) noexcept
{
    #ifdef DEBUG
    assert(fd >= 0 && "recvmsg: fd is invalid");
    assert(msg != nullptr && "recvmsg: msg is nullptr");
    #endif
    return detail::io_sendmsg{loop, fd, msg, bytes_sent, flags, complete_by};
}

/**
 * @} // io_net
 */
} // namespace io
