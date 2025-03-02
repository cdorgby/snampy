#pragma once

#include <coroutine>
#include <io/common.hpp>
#include <io/iotask.hpp>
#include <io/file_descriptor.hpp>

namespace io
{

namespace detail
{

enum class poll_result
{
    success,
    timeout,
    error,
};

// Forward declare io_waiter class which is defined in waiter.hpp
class io_waiter;

enum class io_loop_state
{
    running, //!< The loop is running.
    stop,    //!< The loop has been requested to stop.
    stopped, //!< The loop has stopped.

    shutting_down, //!< The loop is shutting down.
    shutdown,     //!< The loop has stopped and is shut down. It cannot be restarted.
};

template <typename poller_type>
class io_loop_basic
{
  public:
    io_loop_basic() {
        tasks_.reserve(INITIAL_CAPACITY);
        scheduled_.reserve(INITIAL_CAPACITY);
        waiters_.reserve(INITIAL_CAPACITY);
    }
    ~io_loop_basic() = default;

    io_loop_basic(const io_loop_basic &) = delete;
    io_loop_basic(io_loop_basic &&)      = delete;

    io_loop_basic &operator=(const io_loop_basic &) = delete;
    io_loop_basic &operator=(io_loop_basic &&)      = delete;

    void init();
    void run();
    void stop();

    /**
     * @brief Schedule a generic coroutine to run.
     */
    [[nodiscard]] bool schedule(std::coroutine_handle<> &coro);
    [[nodiscard]] bool schedule(io_task &&task, std::string id);

    /**
     * @brief Return the time when the next timeout should occur.
     * @return time_ticks_t The time when the next timeout will occur.
     */
    [[nodiscard]] constexpr time_point_t next_timeout() const;

    /**
     * @brief Returns the time until the next timeout in ticks.
     * @return time_ticks_t The time until the next timeout in ticks.
     */
    [[nodiscard]] constexpr time_ticks_t next_timeout_ticks() const;

    poll_result poll(time_ticks_t timeout, std::vector<io_waiter *> &ready_waiters);
    void add_waiter(io_waiter *waiter);
    void remove_waiter(io_waiter *waiter);

    /**
     * @brief Get the number of active waiters in the loop.
     * @return The number of active waiters.
     */
    size_t waiter_count() const noexcept {
        return waiters_.size();
    }

  private:
    [[nodiscard]] int step();
    bool process_ready_waiters(std::vector<io_waiter *> &ready_waiters);

  private:
    poller_type poller_;
    io_loop_state state_ = io_loop_state::stopped;
    static constexpr size_t INITIAL_CAPACITY = 64;
    std::vector<io_task> tasks_;
    std::vector<std::coroutine_handle<>> scheduled_;
    std::vector<io_waiter *> waiters_;
};

} // namespace detail

} // namespace io

[[nodiscard]] constexpr const std::string_view to_string(io::detail::io_loop_state state)
{
    switch (state)
    {
    case io::detail::io_loop_state::running: return "running";
    case io::detail::io_loop_state::stop: return "stop";
    case io::detail::io_loop_state::stopped: return "stopped";

    case io::detail::io_loop_state::shutting_down: return "shutting_down";
    case io::detail::io_loop_state::shutdown: return "shutdown";
    default: return "unknown";
    }
}

inline std::ostream &operator<<(std::ostream &os, io::detail::io_loop_state state) { return os << to_string(state); }
