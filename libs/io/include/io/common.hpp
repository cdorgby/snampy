#pragma once

#include <chrono>

namespace io
{
namespace detail
{

class epoll_poller;
template <typename poller_type> class io_loop_basic;

} // namespace detail

using io_loop = io::detail::io_loop_basic<io::detail::epoll_poller>;

using time_ticks_t = std::chrono::duration<int64_t, std::micro>;
using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

inline static time_point_t time_now() noexcept { return std::chrono::steady_clock::now(); }

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

} // namespace io
