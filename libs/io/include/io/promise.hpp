#pragma once

#include <io/waiter.hpp>
#include <io/common.hpp>
#include <io/error_handling.hpp>

namespace io
{
namespace detail
{
/**
 * @brief A promise that allows waiting for an I/O operation to complete.
 * 
 * This implements the awaitable pattern for I/O operations. The caller can co_await on this promise to wait for the
 * operation to complete.
 * 
 * Anybody that has access to the promise can reset it to wait for another operation to complet or cancel it.
 * Cancellation is an immediate action and is meaningful only if the operation is still waiting.
 * 
 * This is meant to be used as a base class, if used directly it only allows waiting for a timeout
 * 
 * @note Error Handling: When an operation completes with io_result::error,
 * the error_ field will contain the specific error code. You can use error()
 * to access this information. For other result types like io_result::timeout,
 * the appropriate error code is also set automatically.
 */
struct io_promise
{
    io_waiter waiter_;

    io_promise()                        = delete;
    io_promise(const io_promise &) = delete;

    virtual ~io_promise() = default;

    io_promise(io_loop &loop, time_point_t complete_by) noexcept : waiter_{loop, nullptr, this, complete_by} {}

    [[nodiscard]] virtual bool await_ready() noexcept
    {
        // Check for timeout first to update result if needed
        if (timeout()) { return true; }
        
        // If already completed with any result, we're ready
        if (waiter_.result() != io_result::waiting) { return true; }

        // Add to the I/O loop to await completion
        waiter_.add();
        return false;
    }

    virtual void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        waiter_.awaiting_coroutine_ = awaiting_coroutine;
    }

    [[nodiscard]] io_result await_resume() noexcept
    {
        // let the promise that we are about to resume and they should do any necessary cleanup
        waiter_.promise_->completed();
        // Always remove from I/O loop when resuming
        waiter_.remove();
        
        auto result = waiter_.result();
        // No need for additional error code logic here - we handle it in the error() method
        return result;
    }

    void reset(time_point_t complete_by = time_point_t::max()) noexcept
    {
        waiter_.reset();
        waiter_.complete_by_ = complete_by;
    }

    bool timeout() noexcept
    {
        if (waiter_.complete_by_ != time_point_t::max() && waiter_.complete_by_ < time_now())
        {
            waiter_.result_ = io_result::timeout;
            error_ = make_error_code(io_errc::operation_timeout);
            return true;
        }
        return false;
    }

    virtual void cancel() noexcept
    {
        error_ = make_error_code(io_errc::operation_aborted);
        waiter_.complete(io_result::cancelled);
    }

    [[nodiscard]] bool canceled() const noexcept
    {
        return waiter_.result() == io_result::cancelled;
    }

    /**
     * @brief Returns the error code associated with this promise.
     * @return The error code, which is valid when result is io_result::error.
     */
    [[nodiscard]] std::error_code error() const noexcept { 
        // If error is already set, return it
        if (has_error()) {
            return error_;
        }
        
        // Otherwise, generate an error code based on the result
        auto result = waiter_.result();
        if (result != io_result::done && result != io_result::waiting) {
            return result_to_error(result);
        }
        
        return {};
    }
    
    /**
     * @brief Checks if an error has been set.
     * @return @e true if an error code is set, @e false otherwise.
     */
    [[nodiscard]] bool has_error() const noexcept { 
        return error_.value() != 0;
    }
    
    /**
     * @brief Sets the error code for this promise.
     * @param ec The error code to set.
     */
    void set_error(std::error_code ec) noexcept {
        error_ = ec;
    }

    /**
     * @brief check if the operation associated with this promise is ready.
     * 
     * This is a non-suspending version of await_ready. It checks if the operation is ready to be resumed. It called
     * from.
     * 
     * A derived class can override this method to provide a custom implementation to check if the operation is ready to
     * complete. (e.g. checking if a socket, or filling a buffer until full/timeout)
     * 
     * The default implementation checks if the operation is in any state other than io_result::waiting. In case of
     * simple operations, this is sufficient to wake up the awaiting coroutine on timeout or any i/o ready state.
     * 
     * @return @e true if the operation is complete and can be resumed ([re]schedule the coroutine)
     * @return @e false will cause the the operation to be added back to the I/O look until it's ready or timed out
     *
     */
    virtual bool check_ready() noexcept { return waiter_.result() != io_result::waiting; }

    /**
     * @brief check if the operation associated with this promise is closed.
     */
    virtual bool check_closed() noexcept { return false; }

    /**
     * @brief Called when the operation is about to be resumed.
     * 
     * This method is called just before the operation is resumed. It can be used to perform any cleanup or finalization
     * before the operation is resumed.
     */
    virtual void completed() noexcept {}

  protected:
    std::error_code error_{};
};

/**
 * @brief A promise that allows waiting for an I/O descriptor to be ready for reading or writing.
 */
struct io_desc_promise : public io_promise
{
    io_desc_promise() = delete;
    io_desc_promise(const io_desc_promise &) = delete;

    io_desc_promise(io_loop &loop, int fd, io_desc_type type, time_point_t complete_by) noexcept
    : io_promise{loop, complete_by}
    {
        waiter_.set_descriptor(fd, type);
    }

    void reset(int fd, io_desc_type type, time_point_t complete_by) noexcept
    {
        waiter_.set_descriptor(fd, type);
        io_promise::reset(complete_by);
    }

    [[nodiscard]] int fd() const noexcept { return waiter_.fd(); }
    [[nodiscard]] io_desc_type type() const noexcept { return waiter_.type(); }

};

/**
 * @brief A promise that allows waiting for a set of promises to complete.
 *
 * This promise will wait for any of the promises to complete or for the specified time to pass before resuming the
 * awaiting coroutine.
 * 
 * The caller still needs to call co_await on the individual promises to get the results.
 */
struct io_wait_for_any_promise : public io_promise
{
    io_wait_for_any_promise() = delete;
    io_wait_for_any_promise(const io_wait_for_any_promise &) = delete;

    // Change reference to pointer in initializer_list
    io_wait_for_any_promise(io_loop &loop, time_point_t complete_by, std::initializer_list<io_promise *> promises) noexcept
    : io_promise{loop, complete_by}
    {
        for (auto *promise : promises)
        {
            promise->waiter_.add(&waiter_);
        }
    }

    [[nodiscard]] bool await_ready() noexcept override
    {
        bool ready = false;

        // don't suspend if any of the waiters are done
        for (auto *waiter : waiter_.waiters_)
        {
            // technically we should only get here if a promise is not null, but check just in case
            if (!waiter->promise_) { continue; }
            if (waiter->promise_->await_ready()) { ready = true; }
        }

        // if one of the waiters is ready, return true
        if (ready) { return true; }

        // finally, suspend if we're not ready
        return waiter_.result() != io_result::waiting;
    }

    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        waiter_.awaiting_coroutine_ = awaiting_coroutine;
    }

    [[nodiscard]] std::vector<io_promise *> await_resume() noexcept
    {
        std::vector<io_promise *> ready_waiters;

        for (auto *waiter : waiter_.waiters_)
        {
            if (!waiter) { continue; }
            
            // Check if we timed out and the child waiter is still waiting
            if (timeout() && waiter->result() == io_result::waiting) {
                // Don't update child waiters to timeout - that will happen when they're awaited
                // Just collect those that completed
                continue;
            }
            
            waiter->awaiting_waiter_ = nullptr;
            waiter->remove();
            
            // Only include completed waiters in results
            if (waiter->result() != io_result::waiting) { 
                ready_waiters.emplace_back(waiter->promise_); 
            }
        }

        waiter_.waiters_.clear();
        waiter_.remove();

        return ready_waiters;
    }

    void cancel() noexcept override
    {
        io_promise::cancel();
        for (auto *waiter : waiter_.waiters_)
        {
            waiter->promise_->cancel();
        }
    }
};

/**
 * @brief A promise that allows waiting for all promises to complete.
 * 
 * This promise will wait for all of the promises to complete or for the specified time to pass before resuming the
 * awaiting coroutine.
 * 
 * The caller still needs to call co_await on the individual promises to get the results.
 */
struct io_wait_for_all_promise : public io_promise
{
    io_wait_for_all_promise() = delete;
    io_wait_for_all_promise(const io_wait_for_all_promise &) = delete;

    // Keep existing constructor for initializer lists
    io_wait_for_all_promise(io_loop &loop, time_point_t complete_by, std::initializer_list<io_promise *> promises) noexcept
    : io_promise{loop, complete_by}
    {
        // Set minimum completion count to 1 for empty lists
        waiter_.set_completion_count(promises.size() > 0 ? promises.size() : 1);
        for (auto *promise : promises)
        {
            promise->waiter_.add(&waiter_);
        }
        
        // For empty list, complete immediately with done status
        if (promises.size() == 0) {
            waiter_.complete(io_result::done);
        }
    }

    // Remove the plain vector constructor and keep only the one with optional parameter
    io_wait_for_all_promise(io_loop &loop, time_point_t complete_by, const std::vector<io_promise *>& promises, 
                           size_t completion_count = 0) noexcept
    : io_promise{loop, complete_by}
    {
        // Use provided completion count or promises size
        waiter_.set_completion_count(completion_count > 0 ? completion_count : 
                                    (promises.size() > 0 ? promises.size() : 1));
        
        for (auto *promise : promises) {
            promise->waiter_.add(&waiter_);
        }
        
        // For empty list, complete immediately with done status
        if (promises.empty()) {
            waiter_.complete(io_result::done);
        }
    }

    [[nodiscard]] bool await_ready() noexcept override
    {
        bool all_ready = true;
        size_t completed_count = 0;

        // Count how many waiters are already completed
        for (auto *waiter : waiter_.waiters_)
        {
            if (!waiter->promise_) { continue; }
            if (waiter->result() != io_result::waiting) { 
                completed_count++;
                continue; 
            }
            all_ready = false;
        }

        // If all waiters are completed or our list is empty, return true
        if (all_ready || waiter_.waiters_.empty()) { 
            return true; 
        }

        // Check if we've timed out
        if (timeout()) { return true; }

        // Add ourselves to the IO loop to wait for completion
        waiter_.add();
        
        // Set the completion count to match the number of waiters that are still waiting
        waiter_.set_completion_count(waiter_.waiters_.size() - completed_count);
        
        LOG(debug) << "io_wait_for_all_promise::await_ready with " << waiter_.waiters_.size() 
                  << " waiters, " << completed_count << " already completed, setting completion_count="
                  << (waiter_.waiters_.size() - completed_count);
        
        // We're not ready yet
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        waiter_.awaiting_coroutine_ = awaiting_coroutine;
    }

    [[nodiscard]] std::vector<io_promise *> await_resume() noexcept
    {
        std::vector<io_promise *> completed_waiters;
        
        // Reserve space based on actual completed waiters to avoid reallocations
        size_t completed_count = 0;
        for (auto* waiter : waiter_.waiters_) {
            if (waiter && waiter->result() != io_result::waiting && waiter->promise_) {
                completed_count++;
            }
        }
        
        completed_waiters.reserve(completed_count);
        
        LOG(debug) << "io_wait_for_all_promise::await_resume with " << waiter_.waiters_.size() 
                  << " waiters, found " << completed_count << " completed";

        // First collect completed waiters
        for (auto *waiter : waiter_.waiters_)
        {
            if (!waiter || !waiter->promise_) { 
                continue; 
            }
            
            // Only include completed waiters
            if (waiter->result() != io_result::waiting) {
                completed_waiters.push_back(waiter->promise_);
            } else if (timeout()) {
                // Force completion of remaining waiters if we timed out
                waiter->complete(io_result::timeout);
                completed_waiters.push_back(waiter->promise_);
            }
            
            // Clear parent reference 
            waiter->awaiting_waiter_ = nullptr;
            
            // Ensure waiter is removed from I/O loop
            waiter->remove();
        }

        // Clear our list of waiters since we've processed them all
        waiter_.waiters_.clear();
        
        // Remove ourselves from the I/O loop
        waiter_.remove();
        
        LOG(debug) << "io_wait_for_all_promise completed with " << completed_waiters.size() 
                  << " completed waiters";
                  
        return completed_waiters;
    }
    
    void cancel() noexcept override
    {
        io_promise::cancel();
        for (auto *waiter : waiter_.waiters_)
        {
            waiter->promise_->cancel();
        }
    }
};

} // namespace detail

auto yield(io_loop &loop) { return detail::io_promise{loop, time_now()}; }
auto sleep(io_loop &loop, std::chrono::milliseconds duration)
{
    return detail::io_promise{loop, time_now() + duration};
}

} // namespace io