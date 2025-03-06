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
    _overrun_flag = false;  // Clear the overrun flag on reset

    if (clear)
    {
        _buf->clear();
    }
}

void io_buf::resize(size_t size)
{
    // can only resize if we own the buffer
    if (&parent_ != this)
    {
        return;
    }

    if (_buf->size() < size)
    {
        _buf->resize(size);
    }
    _buf_size = size;

    // adjust offset if it's out of bounds
    if (_buf_offset + _buf_len > _buf_size)
    {
        _buf_offset = _buf_size - _buf_len;
    }
}

void io_buf::adjust_offset(const io_buf &other)
{
    // Only adjust length of a parent buffer based on child
    if (&parent_ == this)
    {
        // Calculate the child's absolute end position relative to this buffer
        size_t child_offset_relative = other._buf_offset - _buf_offset;
        size_t child_end_pos = child_offset_relative + other._buf_len;
        
        // Extend our readable length if the child has written beyond our current end
        if (child_end_pos > _buf_len)
        {
            _buf_len = child_end_pos;
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

    size_t requested_len = data.size();
    size_t available_len = _buf_size - static_cast<size_t>(offset);
    
    // Check for potential overrun
    if (requested_len > available_len) {
        _overrun_flag = true;
    }
    
    size_t len = std::min(requested_len, available_len);
    
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

    // Check if we're trying to write past the buffer size
    if (offset + 1ULL > _buf_size)
    {
        _overrun_flag = true;  // Set the overrun flag
        return 0;
    }

    *reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset) = value;

    if (advance && static_cast<size_t>(offset) + 1ULL > _buf_len)
    {
        _buf_len = static_cast<size_t>(offset) + 1ULL;
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

    // Check if we're trying to write past the buffer size
    if (offset + 2ULL > _buf_size)
    {
        _overrun_flag = true;  // Set the overrun flag
        return 0;
    }

    // Convert from host byte order to network byte order
    uint16_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 2ULL > _buf_len)
    {
        _buf_len = static_cast<size_t>(offset) + 2ULL;
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

    // Check if we're trying to write past the buffer size
    if (offset + 4ULL > _buf_size)
    {
        _overrun_flag = true;  // Set the overrun flag
        return 0;
    }

    // Convert from host byte order to network byte order
    uint32_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 4ULL > _buf_len)
    {
        _buf_len = static_cast<size_t>(offset) + 4ULL;
    }

    return 4;
}

size_t io_buf::write64(uint64_t value, off_t offset, bool advance)
{
    offset = normalize_write_offset(offset);
    if (offset < 0) {
        return 0;
    }

    // Check if we're trying to write past the buffer size
    if (offset + 8ULL > _buf_size)
    {
        _overrun_flag = true;  // Set the overrun flag
        return 0;
    }

    // Convert from host byte order to network byte order
    uint64_t network_value = byte_order::host_to_network(value);
    auto ptr = reinterpret_cast<uint8_t *>(_buf->data() + _buf_offset + offset);

    // Write directly to memory
    memcpy(ptr, &network_value, sizeof(network_value));

    if (advance && static_cast<size_t>(offset) + 8ULL > _buf_len)
    {
        _buf_len = static_cast<size_t>(offset) + 8ULL;
    }

    return 8;
}

std::string io_buf::to_string() const
{
    std::stringstream os;
    std::ios_base::fmtflags original_flags(os.flags());
    
    const char* data = read_ptr();
    size_t len = readable();
    
    using namespace io::detail;
    
    bool last_line_same = false;
    std::string last_line_content;  // Store content without the offset
    
    os << "len: " << len << "\n";
    
    // Set hex formatting here before any hex output
    os << std::hex << std::setfill('0');
    
    for (size_t offset = 0; offset < len; offset += BYTES_PER_LINE) {
        // First, build the content for comparison (without offsets)
        std::stringstream current_line;
        
        // Check if this line has the same content as the previous one
        bool is_duplicate = false;
        if (offset > 0) {  // Skip the first line check
            // Compare the raw bytes between this line and the previous line
            size_t bytes_to_check = std::min(BYTES_PER_LINE, len - offset);
            size_t prev_bytes = std::min(BYTES_PER_LINE, std::min(len - (offset - BYTES_PER_LINE), (size_t)BYTES_PER_LINE));
            
            if (bytes_to_check == prev_bytes && 
                memcmp(data + offset, data + offset - BYTES_PER_LINE, bytes_to_check) == 0) {
                is_duplicate = true;
            }
        }
        
        if (is_duplicate) {
            // This line has the same content as the previous line
            if (!last_line_same) {
                os << "*\n";
                last_line_same = true;
            }
        } else {
            // This line is different, format and output it
            std::stringstream current_line;
            current_line << std::hex << std::setfill('0');
            current_line << std::setw(8) << offset << "  ";

            // Write hex values with proper spacing
            for (size_t i = 0; i < BYTES_PER_LINE; i++) {
                if (i == 8) {
                    current_line << " ";
                }
                
                if (offset + i < len) {
                    current_line << std::setw(BYTE_DISPLAY_WIDTH)
                              << static_cast<unsigned>(static_cast<unsigned char>(data[offset + i])) << " ";
                } else {
                    current_line << "   ";  // 3 spaces for each missing byte
                }
            }

            // Add ASCII representation
            current_line << " |";  // Add extra space before the pipe
            for (size_t i = 0; i < BYTES_PER_LINE && (offset + i) < len; i++) {
                unsigned char c = static_cast<unsigned char>(data[offset + i]);
                current_line << (is_printable(c) ? static_cast<char>(c) : UNPRINTABLE_CHAR);
            }
            current_line << "|";
            
            os << current_line.str() << "\n";
            last_line_same = false;
        }
    }

    os.flags(original_flags);
    return os.str();
}

} // namespace io


