#pragma once

#include <chrono>

namespace io
{

namespace detail
{

class epoll_poller;
template <typename poller_type> class io_loop_basic;

enum io_desc_type
{
    none  = 0, // No I/O operation - timeout when being processed by process_ready_waiters()
    read  = 1,
    write = 2,
    both  = 3,

};

} // namespace detail

using io_loop = io::detail::io_loop_basic<io::detail::epoll_poller>;

using time_ticks_t = std::chrono::duration<int64_t, std::micro>;
using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

/**
 * @brief Possible outcomes of an I/O waiter.
 */
enum class io_result : int
{
    /// @brief The I/O operation is still waiting for completion.
    waiting = 0,
    /// @brief The I/O operation completed successfully.
    done,
    /// @brief The I/O operation timed out.
    timeout,
    /// @brief The I/O operation resulted in an error.
    error,
    /// @brief The underling I/O descriptor was closed.
    closed,
    /// @brief The I/O operation was cancelled, can't resume without a reset.
    cancelled,
    /// @brief The I/O loop is shutting down, operation can be resumed.
    shutdown,
};

inline static time_point_t time_now() noexcept { return std::chrono::steady_clock::now(); }

[[nodiscard]] constexpr int to_int(io_result result) noexcept { return static_cast<int>(result); }

template <typename T> inline const std::string to_string(T result) noexcept;

template <> inline const std::string to_string(io_result result) noexcept
{
    switch (result)
    {
    case io_result::waiting: return "waiting";
    case io_result::done: return "done";
    case io_result::timeout: return "timeout";
    case io_result::error: return "error";
    case io_result::closed: return "closed";
    case io_result::cancelled: return "cancelled";
    case io_result::shutdown: return "shutdown";
    }
    return "unknown";
}

template<>
inline const std::string to_string(io::detail::io_desc_type type) noexcept
{
    switch (type)
    {
    case io::detail::io_desc_type::none: return "none";
    case io::detail::io_desc_type::read: return "read";
    case io::detail::io_desc_type::write: return "write";
    case io::detail::io_desc_type::both: return "both";
    }
    return "unknown";
}

} // namespace io
