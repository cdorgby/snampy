#pragma once

#include <unistd.h>

namespace io {
namespace detail {

class file_descriptor
{
public:
    explicit file_descriptor(int fd = -1) : fd_(fd) {}
    ~file_descriptor()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    file_descriptor(const file_descriptor &) = delete;
    file_descriptor &operator=(const file_descriptor &) = delete;

    file_descriptor(file_descriptor &&other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    file_descriptor &operator=(file_descriptor &&other) noexcept
    {
        if (this != &other)
        {
            if (fd_ != -1)
            {
                close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }
    void reset(int fd = -1)
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

} // namespace detail
} // namespace io
