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
    bool executed_ = false;  // Track whether execute() has been called

    io_op_base(io_loop &loop, int fd, io_desc_type type, time_point_t complete_by = time_point_t::max()) noexcept
    : io_desc_promise{loop, fd, type, complete_by}
    {
    }

    void reset(int fd, io_desc_type type, time_point_t complete_by) noexcept
    {
        io_desc_promise::reset(fd, type, complete_by);
        executed_ = false;
    }

    // Add accessors for error information
    [[nodiscard]] const std::error_code &error() const noexcept { return error_; }

    [[nodiscard]] bool has_error() const noexcept { return error_.value() != 0; }

    [[nodiscard]] std::string error_message() const noexcept { return error_.message(); }

};

} // namespace detail
} // namespace io
