#pragma once

#include <ranges>

#include <common/log.hpp>
#include <io/io_loop.hpp>

namespace io
{

namespace detail
{

template <typename poller_type>
bool io_loop_basic<poller_type>::schedule(std::coroutine_handle<> &coro)
{
    for (auto it = scheduled_.begin(); it != scheduled_.end(); ++it)
    {
        if (*it == coro) { return false; }
    }
    scheduled_.emplace_back(coro);

    return true;
}

template <typename poller_type>
bool io_loop_basic<poller_type>::schedule(io_task &&task, std::string id)
{
    auto handle = task.handle();
    auto it = std::ranges::find_if(tasks_, [&handle](const auto &t) { return t.handle() == handle; });
    bool found = (it != tasks_.end());

    if (!found)
    {
        task.set_task_id(id);
        tasks_.emplace_back(std::move(task));
        handle = tasks_.back().handle();

        LOG(trace) << "Scheduled new task " << id << " with handle " << handle.address();
    }

    // check if the task is already scheduled
    for (auto it = scheduled_.begin(); it != scheduled_.end(); ++it)
    {
        if (*it == handle) { return false; }
    }

    scheduled_.emplace_back(handle);

    return true;
}

template <typename poller_type>
int io_loop_basic<poller_type>::step()
{
    int count = 0;
    std::vector<std::coroutine_handle<>> scheduled;
    std::vector<std::coroutine_handle<>> finished;

    // keep running tasks until there are no more scheduled tasks
    while(!scheduled_.empty())
    {
        scheduled.clear();
        std::swap(scheduled_, scheduled);

        for (auto handle : scheduled)
        {
            if (!handle) continue;
            
            if (!handle.done())
            {
                handle.resume();
                ++count;
                
                // Only mark as finished if done
                if (handle.done()) {
                    finished.emplace_back(handle);
                }
            }
            else {
                finished.emplace_back(handle);
            }
        }
    }

    // Process finished tasks in order they completed
    if (!finished.empty()) {
        LOG(trace) << "Cleaning up " << finished.size() << " finished tasks";

        for (auto handle : finished) {
            if (!handle) continue;
            
            // Find and remove from tasks_ vector if present
            for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
                if (it->handle() == handle) {
                    LOG(trace) << "Destroying task " << it->task_id() << " with handle " << handle.address();
                    tasks_.erase(it);
                    break;
                }
            }
        }
    }

    return count;
}

template <typename poller_type>
constexpr time_point_t io_loop_basic<poller_type>::next_timeout() const {
    time_point_t next = time_point_t::max();
    
    for (const auto* waiter : waiters_) {
        if (waiter->result_ == io_result::waiting && 
            waiter->complete_by_ != time_point_t::max()) {
            next = std::min(next, waiter->complete_by_);
        }
    }
    
    return next;
}

template <typename poller_type>
constexpr time_ticks_t io_loop_basic<poller_type>::next_timeout_ticks() const
{
    time_point_t next_to = next_timeout();

    if (next_to == time_point_t::max()) { return time_ticks_t::max(); }

    auto duration = next_to - time_now();
    
    // If timeout is in the past, return 0 (immediate timeout)
    if (duration.count() < 0) {
        return time_ticks_t(0);
    }
    
    return std::chrono::duration_cast<time_ticks_t>(duration);
}

template <typename poller_type> void io_loop_basic<poller_type>::init() { poller_.init(); }

template <typename poller_type>
void io_loop_basic<poller_type>::run()
{
    std::vector<io_waiter *> ready_waiters;

    if (state_ == io_loop_state::shutdown)
    {
        LOG(error) << "io_loop is shutdown, cannot run";
        return;
    }

    poller_.init();
    state_ = io_loop_state::running;

    while (state_ == io_loop_state::running || state_ == io_loop_state::shutting_down)
    {
        step();

        LOG(trace) << "Tasks: " << tasks_.size() << ", Scheduled: " << scheduled_.size() << ", Waiters: " << waiters_.size();
        // Check if we're done - all tasks completed AND no more waiters
        if (tasks_.empty() && scheduled_.empty() && waiters_.empty())
        {
            LOG(debug) << "No tasks, scheduled, or waiters, stopping loop";
            state_ = io_loop_state::stop;
            break;
        }
        

        time_ticks_t timeout = next_timeout_ticks();

        if (timeout == time_ticks_t::max())
        {
            LOG(trace) << "No timeout";
        }
        else
        {
            LOG(trace) << "Next timeout: " << timeout.count() << "us";
        }
        poller_.poll(timeout, ready_waiters);

        // check for timeouts - handle immediate timeouts for all waiters that are past their deadline
        auto now = time_now();
        for (auto it = waiters_.begin(); it != waiters_.end(); ++it)
        {
            if ((*it)->result_ != io_result::waiting) { continue; }

            // If waiter has a timeout and it's already passed or less than 1ms remains
            if ((*it)->complete_by_ != time_point_t::max() && ((*it)->complete_by_ - now < std::chrono::milliseconds(1)))
            {
                ready_waiters.emplace_back(*it);
                LOG(trace) << "waiter timeout";
            }
        }

        process_ready_waiters(ready_waiters);
    }
    
    // Clean up any remaining tasks
    if (!tasks_.empty()) {
        LOG(debug) << "Cleaning up " << tasks_.size() << " remaining tasks";
        tasks_.clear();
    }
}

template <typename poller_type> void io_loop_basic<poller_type>::stop()
{
    if (state_ == io_loop_state::running) { state_ = io_loop_state::stop; }
}

template <typename poller_type>
poll_result io_loop_basic<poller_type>::poll(time_ticks_t timeout, std::vector<io_waiter *> &ready_waiters)
{
    return poller_.poll(timeout, ready_waiters);
}

template <typename poller_type> void io_loop_basic<poller_type>::add_waiter(io_waiter *waiter)
{
    waiters_.emplace_back(waiter);
    poller_.add_waiter(waiter);
}

template <typename poller_type> void io_loop_basic<poller_type>::remove_waiter(io_waiter *waiter)
{
    poller_.remove_waiter(waiter);

    // check if the waiter is scheduled and remove it
    for (auto it = scheduled_.begin(); it != scheduled_.end(); ++it)
    {
        if (*it == waiter->awaiting_coroutine_)
        {
            scheduled_.erase(it);
            break;
        }
    }

    // now remove the waiter from the list
    for (auto it = waiters_.begin(); it != waiters_.end(); ++it)
    {
        if (*it == waiter)
        {
            waiters_.erase(it);
            return;
        }
    }
}

template <typename poller_type>
bool io_loop_basic<poller_type>::process_ready_waiters(std::vector<io_waiter *> &ready_waiters)
{
    LOG(trace) << "Processing " << ready_waiters.size() << " ready waiters";
    
    for (auto it = ready_waiters.begin(); it != ready_waiters.end(); ++it)
    {
        io_waiter *waiter = *it;

        if (waiter->result() != io_result::waiting) 
        { 
            LOG(trace) << "Skipping waiter with result " << static_cast<int>(waiter->result());
            continue; 
        } // somebody else already completed this waiter

        // ready set to none means timeout
        if (waiter->ready() == io_desc_type::none) 
        { 
            LOG(trace) << "Waiter timed out, completing with timeout";
            waiter->complete(io_result::timeout); 
        }
        else
        {
            LOG(trace) << "Waiter is ready with type " << static_cast<int>(waiter->ready());
            
            if (waiter->promise_)
            {
                LOG(trace) << "Waiter has promise, checking ready state";
                // promise wants to complete or still wants to wait some more
                if (waiter->promise_->check_ready())
                {
                    // handle propogation of the result
                    if (waiter->promise_->check_closed()) 
                    { 
                        LOG(trace) << "Promise closed, completing with closed";
                        waiter->complete(io_result::closed); 
                    }
                    else if (waiter->promise_->has_error()) 
                    { 
                        LOG(trace) << "Promise has error, completing with error";
                        waiter->complete(io_result::error); 
                    }
                    else 
                    { 
                        LOG(trace) << "Promise completed successfully";
                        waiter->complete(io_result::done); 
                    }
                }
                else
                {
                    LOG(trace) << "Promise not ready yet";
                }
            }
            else
            {
                // No promise, just complete the waiter
                LOG(trace) << "Waiter has no promise, completing with done";
                waiter->complete(io_result::done);
            }
        }

        waiter->clear_ready();
        LOG(trace) << "Cleared ready state for waiter";
    }

    ready_waiters.clear();
    LOG(trace) << "Cleared ready waiters list";

    return true;
}

} // namespace detail
} // namespace io