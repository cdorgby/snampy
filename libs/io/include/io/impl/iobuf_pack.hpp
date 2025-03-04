#pragma once

#include <iomanip>
#include <sstream>
#include <cstring>
#include <io/iobuf.hpp>
#include <type_traits>

namespace io {

// Helper type for static_assert - must be declared BEFORE use
template <typename>
struct always_false : std::false_type {};

// Add a primary template that static_asserts on unsupported types
template <typename T> 
size_t io_buf::pack_size(const T &value) {
    static_assert(always_false<T>::value, "Unsupported type for pack_size");
    return 0;
}

// Base template for integral types to reduce code duplication
template <typename T>
typename std::enable_if_t<std::is_integral_v<T>, size_t>
pack_size_integral(T value) {
    if constexpr (std::is_unsigned_v<T>) {
        // Unsigned type logic
        if (value <= IO_BUF_PACK_FIXUINT_MAX) {
            return 1;
        }
        else if (value <= UINT8_MAX) {
            return 2;
        }
        else if (value <= UINT16_MAX) {
            return 3;
        }
        else if (value <= static_cast<T>(UINT32_MAX)) { 
            return 5;
        }
        else {
            return 9;
        }
    } else {
        // Signed type logic
        if (value >= 0) {
            if (value <= IO_BUF_PACK_FIXUINT_MAX) {
                return 1;
            }
            else if (value <= static_cast<T>(UINT8_MAX)) {
                return 2;
            }
            else if (value <= static_cast<T>(UINT16_MAX)) {
                return 3;
            }
            else if (value <= static_cast<T>(INT32_MAX)) {  // Use INT32_MAX instead of UINT32_MAX for signed values
                return 5;
            }
            else {
                return 9;
            }
        } else {
            // Negative value handling
            if (value >= -32) {
                return 1;
            }
            else if (value >= INT8_MIN) {
                return 2;
            }
            else if (value >= INT16_MIN) {
                return 3;
            }
            else if (value >= INT32_MIN) {
                return 5;
            }
            else {
                return 9;
            }
        }
    }
}

// Specializations for integral types using the helper template
template <> size_t io_buf::pack_size(const int64_t &value) {
    return pack_size_integral(value);
}

// This specialization handles both int and int32_t on most platforms
template <> size_t io_buf::pack_size(const int32_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const int16_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const int8_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const uint64_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const uint32_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const uint16_t &value) {
    return pack_size_integral(value);
}

template <> size_t io_buf::pack_size(const uint8_t &value) {
    return pack_size_integral(value);
}

// Remove the duplicate specialization for int type
// template <> size_t io_buf::pack_size(const int &value) {
//     return pack_size_integral(value);
// }

template <> size_t io_buf::pack_size([[maybe_unused]] const bool &value)
{
    return 1;
}

template <> size_t io_buf::pack_size([[maybe_unused]] const decltype(nullptr) &value)
{
    return 1;
}

template <> size_t io_buf::pack_size([[maybe_unused]] const float &value)
{
    return 5;
}

template <> size_t io_buf::pack_size([[maybe_unused]] const double &value)
{
    return 9;
}

template <> size_t io_buf::pack_size(const std::string &value)
{
    if (value.size() <= IO_BUF_PACK_FIXSTR_MAX - IO_BUF_PACK_FIXSTR + 1)
    {
        return 1 + value.size();
    }
    else if (value.size() <= UINT8_MAX)
    {
        return 2 + value.size();
    }
    else if (value.size() <= UINT16_MAX)
    {
        return 3 + value.size();
    }
    else if (value.size() <= UINT32_MAX)
    {
        return 5 + value.size();
    }
    else
    {
        return 9 + value.size();
    }
}

template <> size_t io_buf::pack_size(const char *value)
{
    return pack_size(std::string(value));
}

template <typename T> 
size_t io_buf::pack_size(const std::vector<T>& value)
{
    size_t size = 0;
    if (value.size() <= IO_BUF_PACK_FIXARRAY_MAX - IO_BUF_PACK_FIXARRAY + 1)
    {
        size += 1;
    }
    else if (value.size() <= UINT8_MAX)
    {
        size += 2;
    }
    else if (value.size() <= UINT16_MAX)
    {
        size += 3;
    }
    else if (value.size() <= UINT32_MAX)
    {
        size += 5;
    }
    else
    {
        size += 9;
    }

    for (const auto& item : value)
    {
        size += pack_size(item);
    }
    return size;
}

// treat std::vector<uint8_t> as binary blob
template <> size_t io_buf::pack_size(const std::vector<uint8_t> &value)
{
    size_t size = 0;

    if (value.size() <= IO_BUF_PACK_FIXBIN_MAX - IO_BUF_PACK_FIXBIN + 1)
    {
        size += 1;
    }
    else if (value.size() <= UINT8_MAX)
    {
        size += 2;
    }
    else if (value.size() <= UINT16_MAX)
    {
        size += 3;
    }
    else if (value.size() <= UINT32_MAX)
    {
        size += 5;
    }
    else
    {
        size += 9;
    }

    size += value.size();
    return size;
}

// and std::vector<char> as array of strings
template <> size_t io_buf::pack_size(const std::vector<char> &value)
{
    return pack_size(std::vector<uint8_t>(value.begin(), value.end()));
}

template <typename T>
size_t io_buf::pack(const std::vector<T>& value, off_t offset, bool advance)
{
    size_t written = 0;
    auto count = value.size();

    // Write array header
    if (count <= IO_BUF_PACK_FIXARRAY_MAX - IO_BUF_PACK_FIXARRAY + 1)
    {
        written += write8(IO_BUF_PACK_FIXARRAY + count, offset + written, advance);
    }
    else if (count <= UINT8_MAX)
    {
        written += write8(IO_BUF_PACK_ARRAY8, offset + written, advance);
        written += write8(count, offset + written, advance);
    }
    else if (count <= UINT16_MAX)
    {
        written += write8(IO_BUF_PACK_ARRAY16, offset + written, advance);
        written += write16(count, offset + written, advance);
    }
    else if (count <= UINT32_MAX)
    {
        written += write8(IO_BUF_PACK_ARRAY32, offset + written, advance);
        written += write32(count, offset + written, advance);
    }

    // Write array elements
    for (const auto& item : value)
    {
        written += pack(item, offset + written, advance);
    }

    return written;
}

// Optimize pack/unpack for binary data
template <> size_t io_buf::pack(const std::vector<uint8_t>& value, off_t offset, bool advance)
{
    size_t written = 0;
    auto count = value.size();

    // Write binary header with optimized type selection
    if (count <= IO_BUF_PACK_FIXBIN_MAX - IO_BUF_PACK_FIXBIN + 1) {
        written += write8(IO_BUF_PACK_FIXBIN + count - 1, offset + written, advance);
    }
    else if (count <= UINT8_MAX) {
        written += write8(IO_BUF_PACK_BIN8, offset + written, advance);
        written += write8(count, offset + written, advance);
    }
    else if (count <= UINT16_MAX) {
        written += write8(IO_BUF_PACK_BIN16, offset + written, advance);
        written += write16(count, offset + written, advance);
    }
    else {
        written += write8(IO_BUF_PACK_BIN32, offset + written, advance);
        written += write32(count, offset + written, advance);
    }

    // Write binary data in a single operation
    if (count > 0) {
        written += write(reinterpret_cast<const char*>(value.data()), count, offset + written, advance);
    }

    return written;
}

template <typename T>
size_t io_buf::unpack(std::vector<T>& value, off_t offset, bool advance)
{
    size_t read = 0;
    uint8_t type;
    read += read8(type, offset + read, advance);

    size_t count = 0;
    if (type >= IO_BUF_PACK_FIXARRAY && type <= IO_BUF_PACK_FIXARRAY_MAX)
    {
        count = type - IO_BUF_PACK_FIXARRAY;
    }
    else switch (type)
    {
        case IO_BUF_PACK_ARRAY8:
        {
            uint8_t val;
            read += read8(val, offset + read, advance);
            count = val;
            break;
        }
        case IO_BUF_PACK_ARRAY16:
        {
            uint16_t val;
            read += read16(val, offset + read, advance);
            count = val;
            break;
        }
        case IO_BUF_PACK_ARRAY32:
        {
            uint32_t val;
            read += read32(val, offset + read, advance);
            count = val;
            break;
        }
        default:
            return 0;
    }

    value.resize(count);
    for (size_t i = 0; i < count; i++)
    {
        read += unpack(value[i], offset + read, advance);
    }

    return read;
}

} // namespace io