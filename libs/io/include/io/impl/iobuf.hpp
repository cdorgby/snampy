#pragma once
#include <iomanip>
#include <sstream>
#include <cstring>
#include <io/iobuf.hpp>
#include <io/byte_order.hpp>
#include <ranges>
#include <arpa/inet.h>  // for network byte order functions

namespace io
{

namespace detail {
    constexpr size_t BYTES_PER_LINE = 16;
    constexpr size_t BYTE_DISPLAY_WIDTH = 2;
    constexpr char UNPRINTABLE_CHAR = '.';
    
    inline bool is_printable(unsigned char c) {
        return c >= 32 && c <= 126;
    }
}

void io_buf::reset(bool clear)
{
    _buf_len = 0;
    _buf_offset = _buf_offset_orig;

    if (clear)
    {
        _buf->clear();
    }
}

void io_buf::adjust_offset(const io_buf &other)
{
    if (&_parent == this)
    {
        if (other._buf_offset + other._buf_len > _buf_len)
        {
            _buf_len = other._buf_offset + other._buf_len;
        }
    }
}

char *io_buf::read_ptr(off_t offset) const
{
    off_t off = normalize_read_offset(offset);
    if (off < 0)
    {
        return nullptr;
    }

    if (static_cast<size_t>(off) >= _buf_len)
    {
        return nullptr;
    }

    return _buf->data() + _buf_offset + off;
}

char *io_buf::write_ptr(off_t offset)
{
    off_t off = normalize_write_offset(offset);
    if (off < 0)
    {
        return nullptr;
    }

    if (static_cast<size_t>(off) >= _buf_size)
    {
        return nullptr;
    }

    return _buf->data() + _buf_offset + off;
}

void io_buf::advance_write_ptr(size_t len)
{
    if (_buf_len + len > _buf_size)
    {
        return;
    }
    _buf_len += len;
}

void io_buf::advance_read_ptr(size_t len)
{
    if (_buf_len < len)
    {
        return;
    }

    _buf_offset += len;
    _buf_len -= len;
}

off_t io_buf::normalize_write_offset(off_t offset) const
{
    // Simplified logic with early return for common case
    if (offset >= 0) {
        return (static_cast<size_t>(offset) <= _buf_size) ? offset : -1;
    }
    // Handle negative offsets (relative to current write position)
    offset = _buf_len + (offset + 1);
    return (offset >= 0) ? offset : -1;
}

off_t io_buf::normalize_read_offset(off_t offset) const
{
    // Simplified logic with early return for common case
    if (offset >= 0) {
        return (static_cast<size_t>(offset) <= _buf_len) ? offset : -1;
    }
    // Handle negative offsets (relative to current read position)
    offset = _buf_len + offset;
    return (offset >= 0) ? offset : -1;
}

size_t io_buf::write(const char *data, size_t len, off_t offset, bool advance)
{
    // Simply delegate to the string_view version for consistency
    return write(std::string_view(data, len), offset, advance);
}

size_t io_buf::write(std::string_view data, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);
    if (offset < 0) {
        return 0;
    }

    size_t len = std::min(data.size(), _buf_size - static_cast<size_t>(offset));
    IO_BUF_ASSERT_BOUNDS(len <= data.size(), "Buffer overflow in write()");
    
    memcpy(_buf->data() + _buf_offset + offset, data.data(), len);

    if (advance && static_cast<size_t>(offset) + len > _buf_len) {
        _buf_len = static_cast<size_t>(offset) + len;
    }

    return len;
}

size_t io_buf::read(char *data, size_t len, off_t offset, bool advance)
{
    offset = normalize_read_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    len = std::min(len, _buf_len - offset);

    memcpy(data, _buf->data() + _buf_offset + offset, len);

    if (advance && offset == 0)
    {
        _buf_offset += len;
        _buf_len -= len;
    }

    return len;
}

size_t io_buf::read8(uint8_t &value, off_t offset, bool advance)
{
    offset = normalize_read_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 1ULL > _buf_len)
    {
        return 0;
    }

    // Direct byte access is more efficient than memcpy for single bytes
    value = static_cast<uint8_t>(_buf->data()[_buf_offset + offset]);

    if (advance && offset == 0)
    {
        _buf_offset += 1;
        _buf_len -= 1;
    }

    return 1;
}

size_t io_buf::read16(uint16_t &value, off_t offset, bool advance)
{
    offset = normalize_read_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 2ULL > _buf_len)
    {
        return 0;
    }

    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);
    
    // Read bytes directly and convert from network byte order
    uint16_t network_value;
    memcpy(&network_value, ptr, sizeof(network_value));
    value = byte_order::network_to_host(network_value);

    if (advance && offset == 0)
    {
        _buf_offset += 2;
        _buf_len -= 2;
    }

    return 2;
}

size_t io_buf::read32(uint32_t &value, off_t offset, bool advance)
{
    offset = normalize_read_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 4ULL > _buf_len)
    {
        return 0;
    }

    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Read bytes directly and convert from network byte order
    uint32_t network_value;
    memcpy(&network_value, ptr, sizeof(network_value));
    value = byte_order::network_to_host(network_value);

    if (advance && offset == 0)
    {
        _buf_offset += 4;
        _buf_len -= 4;
    }

    return 4;
}

size_t io_buf::read64(uint64_t &value, off_t offset, bool advance)
{
    offset = normalize_read_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 8ULL > _buf_len)
    {
        return 0;
    }

    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Read bytes directly and convert from network byte order
    uint64_t network_value;
    memcpy(&network_value, ptr, sizeof(network_value));
    value = byte_order::network_to_host(network_value);

    if (advance && offset == 0)
    {
        _buf_offset += 8;
        _buf_len -= 8;
    }

    return 8;
}

size_t io_buf::write8(uint8_t value, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 1ULL > _buf_size)
    {
        return 0;
    }

    *reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset) = value;

    if (advance && static_cast<size_t>(offset) + 1ULL >= _buf_len)
    {
        auto advance_len = static_cast<size_t>(offset) + 1ULL - _buf_len;
        _buf_len += advance_len;
    }

    return 1;
}

size_t io_buf::write16(uint16_t value, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 2ULL > _buf_size)
    {
        return 0;
    }

    // Convert from host byte order to network byte order
    uint16_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 2ULL >= _buf_len)
    {
        auto advance_len = static_cast<size_t>(offset) + 2ULL - _buf_len;
        _buf_len += advance_len;
    }

    return 2;
}

size_t io_buf::write32(uint32_t value, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);

    if (offset < 0)
    {
        return 0;
    }

    if (offset + 4ULL > _buf_size)
    {
        return 0;
    }

    // Convert from host byte order to network byte order
    uint32_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 4ULL >= _buf_len)
    {
        auto advance_len = static_cast<size_t>(offset) + 4ULL - _buf_len;
        _buf_len += advance_len;
    }

    return 4;
}

size_t io_buf::write64(uint64_t value, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);
    if (offset < 0 || offset + 8ULL > _buf_size) {
        return 0;
    }

    // Convert from host byte order to network byte order
    uint64_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 8ULL >= _buf_len)
    {
        auto advance_len = static_cast<size_t>(offset) + 8ULL - _buf_len;
        _buf_len += advance_len;
    }

    return 8;
}

std::ostream &operator<<(std::ostream &os, const io_buf &buf)
{
    std::ios_base::fmtflags original_flags(os.flags());
    os << std::hex << std::setfill('0');

    const char* data = buf.read_ptr();
    size_t len = buf.readable();
    
    // Use std::string_view for line comparison to avoid string copies
    using namespace detail;
    
    bool last_line_same = false;
    std::string_view last_line;
    std::string current_line_buf;  // Buffer for current line
    
    os << "len: " << len << "\n";
    
    for (size_t offset = 0; offset < len; offset += BYTES_PER_LINE) {
        std::stringstream current_line;
        
        current_line << std::setw(8) << offset << "  ";

        for (size_t i = 0; i < BYTES_PER_LINE; i++) {
            if (i == 8) {
                current_line << " ";
            }
            if (offset + i < len) {
                current_line << std::hex << std::setw(BYTE_DISPLAY_WIDTH) << std::setfill('0')
                             << static_cast<unsigned>(static_cast<unsigned char>(data[offset + i])) << " ";
            } else {
                current_line << "   ";
            }
        }

        current_line << " |";
        for (size_t i = 0; i < BYTES_PER_LINE && (offset + i) < len; i++) {
            unsigned char c = data[offset + i];
            current_line << (is_printable(c) ? static_cast<char>(c) : UNPRINTABLE_CHAR);
        }
        current_line << "|\n";

        std::string current_line_str = current_line.str();
        if (offset > 0 && current_line_str == last_line && offset + BYTES_PER_LINE < len) {
            if (!last_line_same) {
                os << "*\n";
                last_line_same = true;
            }
        } else {
            os << current_line_str;
            last_line = current_line_str;
            last_line_same = false;
        }
    }

    os << std::setw(8) << len << "\n";

    os.flags(original_flags);
    return os;
}

} // namespace io
