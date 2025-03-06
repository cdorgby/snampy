#pragma once

#include <io/waiter.hpp>
#include <io/io_loop.hpp>
#include <io/error_handling.hpp>

namespace io {
namespace detail {

bool io_waiter::complete(io_result result) noexcept
{
    // Once we're no longer waiting, ensure we get removed from the I/O loop
    const bool should_remove_from_loop = (result != io_result::waiting);

    result_ = result;

    // Execute callback if set
    if (callback_) { callback_(result, this); }

    // Handle parent waiter
    if (awaiting_waiter_)
    {
        // For io_wait_for_all_promise, we need to decrement its completion count
        // but only proceed to complete it when completion_count_ reaches 0
        if (result != io_result::waiting)
        {
            // For io_wait_for_any_promise, complete the parent immediately with the first completion
            bool complete_parent = true;

            // Only check completion count if it's greater than 1 (wait-for-all behavior)
            if (awaiting_waiter_->completion_count_ > 1)
            {
                // Atomically decrement the parent's completion count and check if it's zero
                complete_parent = (--awaiting_waiter_->completion_count_ == 0);
            }

            // Complete the parent waiter if conditions are met
            if (complete_parent) { awaiting_waiter_->complete(io_result::done); }
        }
    }
    else if (awaiting_coroutine_)
    {
        // Decrement completion count for ANY final state (not just done)
        if (result != io_result::waiting)
        {
            completion_count_--;
            LOG(trace) << "Decremented completion count to " << completion_count_;
        }

        // Schedule coroutine when:
        // 1. All required waiters have completed or
        // 2. Any non-success result occurred and we're not already scheduled
        const bool should_schedule = (completion_count_ == 0) || (result_ != io_result::waiting && !scheduled_);

        if (should_schedule && !scheduled_)
        {
            LOG(trace) << "Scheduling coroutine with result=" << to_string(result) << ", completion_count=" << completion_count_;
            loop_.schedule(awaiting_coroutine_);
            scheduled_          = true;
            awaiting_coroutine_ = nullptr;
        }
    }

    return should_remove_from_loop;
}

void io_waiter::add(io_waiter *awaiting_waiter) noexcept
{
    if (added_) {
        LOG(debug) << "Waiter " << this << " already added, skipping";
        return; 
    }

    if (awaiting_waiter)
    {
        awaiting_waiter_ = awaiting_waiter;
        // Reserve space to avoid reallocation
        awaiting_waiter_->waiters_.reserve(awaiting_waiter_->waiters_.size() + 1);
        awaiting_waiter_->waiters_.push_back(this);
    }

    loop_.add_waiter(this);
    added_ = true;
}

void io_waiter::remove() noexcept
{
    if (!added_) {
        LOG(trace) << "Waiter " << this << " not added, skipping removal";
        return; 
    }

    if (awaiting_waiter_)
    {
        auto &waiters = awaiting_waiter_->waiters_;
        waiters.erase(std::remove(waiters.begin(), waiters.end(), this), waiters.end());
        awaiting_waiter_ = nullptr;
    }

    loop_.remove_waiter(this);
    added_ = false;
}

} // namespace detail
} // namespace io