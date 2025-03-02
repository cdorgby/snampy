#pragma once

#include <io/common.hpp>

#include <io/io_loop.hpp>
#include <io/waiter.hpp>

namespace io {
namespace detail {

struct io_op_base : public io_desc_promise
{
    io_op_base()                   = delete;
    io_op_base(const io_op_base &) = delete;
    std::error_code error_{};
    bool executed_ = false;  // Track whether execute() has been called

    io_op_base(io_loop &loop, int fd, io_desc_type type, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_promise{loop, fd, type, complete_by}
    {
    }

    [[nodiscard]] bool await_ready() noexcept override
    {
        if (timeout()) { return true; }

        // Check if the operation is already done
        if (waiter_.result() != io_result::waiting) { return true; }

        // Check if the fd is ready for this operation type (read/write)
        // but DON'T perform the actual I/O operation yet
        if (check_ready())
        {
            // If ready, try to execute the operation immediately
            return true;
        }

        // Not ready or couldn't complete immediately, need to suspend
        waiter_.add();
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        waiter_.awaiting_coroutine_ = awaiting_coroutine;
    }

    [[nodiscard]] io_result await_resume() noexcept
    {
        waiter_.remove();
        
        // If we timed out or were cancelled, don't try to execute
        if (waiter_.result() == io_result::timeout || 
            waiter_.result() == io_result::cancelled) {
            executed_ = false;  // Reset for next use
            return waiter_.result();
        }

        // Execute the operation if not already executed
        if (waiter_.result() == io_result::done && !executed_) {
            execute();
        }
        
        executed_ = false;  // Reset for next use
        return waiter_.result();
    }

    void reset(int fd, io_desc_type type, time_point_t complete_by) noexcept
    {
        io_desc_promise::reset(fd, type, complete_by);
        executed_ = false;
    }

    /**
     * @brief called from await_ready to perform the action
     *
     * @return @e true if the action completed the operation and it can be resumed immediately
     * @return @e false if the action did not complete the operation and it needs to be awaited
     */
    virtual void execute() noexcept = 0;
    /**
     * @brief called from await_ready to perform the action
     *
     * @return @e true if the action completed the operation and it can be resumed immediately
     * @return @e false if the action did not complete the operation and it needs to be awaited
     */
    virtual bool check_ready() noexcept  = 0;

    // Add accessors for error information
    [[nodiscard]] const std::error_code& error() const noexcept { 
        return error_; 
    }
    
    [[nodiscard]] bool has_error() const noexcept { 
        return error_.value() != 0;
    }
    
    [[nodiscard]] std::string error_message() const noexcept {
        return error_.message();
    }
};

} // namespace detail
} // namespace io
