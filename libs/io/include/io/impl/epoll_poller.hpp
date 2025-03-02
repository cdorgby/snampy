#pragma once

#include <common/log.hpp>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <io/io_loop.hpp>
#include <io/waiter.hpp>
#include <array>

namespace io
{

namespace detail
{

constexpr int max_events = 32;

struct epoll_poller
{
    epoll_poller() = default;
    ~epoll_poller();

    epoll_poller(const epoll_poller &) = delete;
    epoll_poller(epoll_poller &&) = delete;

    epoll_poller &operator=(const epoll_poller &) = delete;
    epoll_poller &operator=(epoll_poller &&) = delete;

    [[nodiscard]] poll_result poll(time_ticks_t timeout, std::vector<io_waiter *> &ready_waiters);
    void add_waiter(class io_waiter *waiter);
    void remove_waiter(class io_waiter *waiter);

    void wake();

    void init() noexcept;

private:
    bool _init();

private:
    bool init_ = false;
    file_descriptor epoll_fd_;
    file_descriptor event_fd_;
};

void epoll_poller::init() noexcept
{
    if (!init_)
    {
        if (_init()) { init_ = true; }
    }
}

epoll_poller::~epoll_poller() = default;

[[nodiscard]] poll_result epoll_poller::poll(time_ticks_t timeout, std::vector<io_waiter *> &ready_waiters) {
    std::array<epoll_event, max_events> events{};
    auto sleep_time = timeout != time_ticks_t::max() 
        ? std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count() 
        : -1;

    if (epoll_fd_.get() == -1 || event_fd_.get() == -1) {
        LOG(error) << "Epoll or eventfd not initialized";
        return poll_result::error;
    }

    int num_events = epoll_wait(epoll_fd_.get(), events.data(), events.size(), sleep_time);
    if (num_events < 0) {
        LOG(error) << "Epoll wait failed";
        return poll_result::error;
    }
    if (num_events == 0) {
        return poll_result::timeout;
    }

    // Process events...
    for (int i = 0; i < num_events; ++i) {
        if (auto *w = static_cast<io_waiter *>(events[i].data.ptr))
        {
            if (events[i].events & EPOLLIN) {
                LOG(trace) << "EPOLLIN";
                w->complete(io_result::done);
            }
            else if (events[i].events & EPOLLOUT) {
                LOG(trace) << "EPOLLOUT";
                w->complete(io_result::done);
            }
            else {
                LOG(error) << "Unknown event: ";
                w->complete(io_result::error);
            }
            ready_waiters.push_back(w);
        }
    }

    return poll_result::success;
}

void epoll_poller::add_waiter(io_waiter *waiter)
{
    if (epoll_fd_.get() == -1)
    {
        LOG(error) << "Epoll not initialized";
        return;
    }

    if (waiter->fd() == -1)
    {
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;

    ev.data.ptr = waiter;
    if (waiter->type() == io_desc_type::read) { ev.events = EPOLLIN; }
    else if (waiter->type() == io_desc_type::write) { ev.events = EPOLLOUT; }
    else if (waiter->type() == io_desc_type::both) { ev.events = EPOLLIN | EPOLLOUT; }

    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, waiter->fd(), &ev) == -1) { LOG(error) << "Failed to add waiter to epoll"; }
}

void epoll_poller::remove_waiter(io_waiter *waiter)
{
    if (epoll_fd_.get() == -1)
    {
        LOG(error) << "Epoll not initialized";
        return;
    }

    if (waiter->fd() == -1)
    {
        return;
    }

    epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, waiter->fd(), nullptr);
}

bool epoll_poller::_init()
{
    if (event_fd_.get() == -1)
    {
        event_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
        if (event_fd_.get() == -1)
        {
            LOG(error) << "Failed to create eventfd";
            return false;
        }
    }

    if (epoll_fd_.get() == -1)
    {
        epoll_fd_.reset(epoll_create1(EPOLL_CLOEXEC));
        if (epoll_fd_.get() == -1)
        {
            LOG(error) << "Failed to create epoll";
            return false;
        }

        struct epoll_event ev;
        ev.events  = EPOLLIN;
        ev.data.fd = event_fd_.get();

        if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, event_fd_.get(), &ev) == -1)
        {
            LOG(error) << "Failed to add eventfd to epoll";
            return false;
        }
    }

    return true;
}

void epoll_poller::wake()
{
    uint64_t val = 1;
    if (event_fd_.get() == -1) {
        LOG(error) << "Eventfd not initialized";
        return;
    }

    if (write(event_fd_.get(), &val, sizeof(val)) == -1) {
        LOG(error) << "Failed to write to eventfd";
    }
}

} // namespace detail
} // namespace io