#pragma once

#include <functional>
#include <coroutine>
#include <vector>
#include <chrono>
#include <atomic>
#include <system_error>

#include <io/common.hpp>
#include <io/error_handling.hpp>

namespace io {

namespace detail {

/**
 * @brief This the I/O loop side of the promise/waiter pattern.
 *
 * This struct represents something that is waiting on a promise to complete. In case of the I/O loop it is any I/O
 * operation that is waiting for a result. Read/write readiness, timeouts, etc.
 *
 * There are two basic types of waiters:
 *
 * A waiter that is waiting for a single operation to complete and it will resume the awaiting coroutine directly.
 *
 * And a waiter that is being awaited on by another waiter. Like the wait_for_any() function. In this case the waiter
 * will resume the `parent's` coroutine when it is completed. Then the caller can check the results of the specific
 * waiters.
 *
 * Generally a waiter is created and owned by a promise.
 * 
 * Waiters have a specific life-cycle:
 * - Created by a promise
 * - Added to the I/O loop
 * - Completed by the I/O loop (i/o, timeout, cancelled, etc.)
 * - Removed from the I/O loop
 * - Wake up the awaiting coroutine
 *
 */

struct io_waiter
{
    // callback used when io_waiter is used without a promise/awaitable
    using io_callback_t = void(*)(io_result, io_waiter *);
    
    // Pre-reserve space for common case
    static constexpr size_t DEFAULT_WAITERS_CAPACITY = 32;

    io_waiter() = delete;
    io_waiter(io_loop &loop, io_callback_t callback, struct io_awaitable *promise, time_point_t complete_by = time_point_t::max()) noexcept
    : complete_by_{complete_by},
      callback_{callback},
      awaitable_{promise},
      loop_{loop} 
    {
        waiters_.reserve(DEFAULT_WAITERS_CAPACITY);
    }

    io_waiter(io_loop &loop, io_waiter *waiter, io_callback_t callback, struct io_awaitable *promise, time_point_t complete_by = time_point_t::max()) noexcept
    : awaiting_waiter_{waiter},
      complete_by_{complete_by},
      callback_{callback},
      awaitable_{promise},
      loop_{loop}
    {
        waiters_.reserve(DEFAULT_WAITERS_CAPACITY);
    }

    ~io_waiter() noexcept {
        remove();

        for (auto *w : waiters_) {
            w->awaiting_waiter_ = nullptr;
        }
    }

    /**
     * @brief The coroutine handle of the operation that is waiting.
     */
    std::coroutine_handle<> awaiting_coroutine_{nullptr};

    /**
     * @brief A pointer to another waiter that is waiting on this operation.
     *
     */
    io_waiter *awaiting_waiter_{nullptr};

    /**
     * @brief List of waiters that could be completed by this operation. (any waiter that has @e awaiting_waiter_ set to
     * this waiter)
     */
    std::vector<io_waiter *> waiters_;

    /**
     * @brief The time point when the operation should be completed.
     *
     * Set to @e {time_point_t::max()} to wait forever. Otherwise, set to the time point when the operation should be
     * completed by.
     */
    time_point_t complete_by_{time_point_t::max()};

    /**
     * @brief The callback to be called when the operation is completed.
     */
    io_callback_t callback_{nullptr};
    /**
     * @brief A pointer to the promise associated with this waiter.
     */
    struct io_awaitable *awaitable_{nullptr};

    /**
     * @brief The result of the operation. What the promise sees
     */
    io_result result_{io_result::waiting};

    /**
     * @brief Arbitrary data associated with the waiter. Can be used by the callback.
     */
    void *data_{nullptr};

    /**
     * @brief Completes the operation and calls the callback, if set.
     * @param result The result of the operation.
     * @param ec Optional error code to be set when result is io_result::error.
     * @return @e true if the the waiter should be removed from the I/O loop. @e false to keep it.
     */
    [[nodiscard]] bool complete(io_result result) noexcept;
    
    [[nodiscard]] io_result result() const noexcept { return result_; }
    [[nodiscard]] bool scheduled() const noexcept { return scheduled_; }
    [[nodiscard]] io_loop &loop() const noexcept { return loop_; }

    [[nodiscard]] int fd() const noexcept { return fd_; }
    [[nodiscard]] io_desc_type type() const noexcept { return type_; }
    [[nodiscard]] io_desc_type ready_reason() const noexcept { return ready_; }

    void set_descriptor(int fd, io_desc_type type) noexcept {
        fd_ = fd;
        type_ = type;
    }

    void reset(time_point_t complete_by = time_point_t::max(), size_t complete_count = 1) noexcept
    {
        // make sure we're not in any list
        remove();

        // Reset all state
        result_           = io_result::waiting;
        complete_by_      = complete_by;
        scheduled_        = false;
        completion_count_ = complete_count;
        awaiting_coroutine_ = nullptr;
    }

    void add(io_waiter *awaiting_waiter = nullptr) noexcept;
    void remove() noexcept;

    void set_completion_count(size_t count) noexcept { completion_count_ = count; }
    void clear_ready() noexcept { ready_ = io_desc_type::none; }
    [[nodiscard]] auto ready() noexcept { return ready_; }

    friend struct epoll_poller;
  private:
    io_loop &loop_;
    /**
     * @brief Flag to indicate if the waiter is scheduled with the I/O loop.
     * 
     * Will be set when the waiter's coroutine is scheduled with the I/O loop.
     */
    bool scheduled_{false};
    /**
     * @brief Flag to indicate if the waiter is added to the I/O loop.
     * 
     * Will be set when the waiter is added to the I/O loop.
     */
    bool added_{false};
    int fd_{-1};  
    io_desc_type type_{io_desc_type::read}; // wanted I/O type (read and/or write)
    io_desc_type ready_{io_desc_type::none}; // ready I/O type (read and/or write)
    size_t completion_count_{1};  // Default to 1 for backward compatibility
};

} // namespace detail
} // namespace io