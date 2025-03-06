#pragma once

#include <common/dll_export.hpp>
#include <stdint.h>
#include <cstddef>
#include <atomic>
#include <memory>
#include <vector>
#include <map>
#include <sys/types.h>
#include <cstring>
#include <iomanip>
#include <string_view>  // Added for string_view support

/**
 * All data is encoded as network ordered (big-endian) numbers.
 * 
 * The format is as follows:
 *
 * Single byte types:
 * +------------+
 * | type/value |
 * +------------+
 *
 * Fixed length types:
 * +------+--------+-----+----------+
 * | type | data+0 | ... | data+len |
 * +------+--------+-----+----------+
 * len is based on the type
 *
 * Strings/Binary data:
 *
 * 8-bit length:
 * +------+-----+--------+-----+----------+
 * | type | len | data+0 | ... | data+len |
 * +------+-----+--------+-----+----------+
 *
 * 16-bit length:
 * +------+-------+-------+--------+-----+----------+
 * | type | len+0 | len+1 | data+0 | ... | data+len |
 * +------+-------+-------+--------+-----+----------+
 *
 * 32-bit length:
 * +------+-------+-------+-------+-------+--------+-----+----------+
 * | type | len+0 | len+1 | len+2 | len+3 | data+0 | ... | data+len |
 * +------+-------+-------+-------+-------+--------+-----+----------+
 *
 * Array:
 * 8-bit length:
 * +------+-----+---------+-----+-----------+
 * | type | len | value+0 | ... | value+len |
 * +------+-----+---------+-----+-----------+
 *
 * 16-bit length:
 * +------+-------+-------+---------+-----+-----------+
 * | type | len+0 | len+1 | value+0 | ... | value+len |
 * +------+-------+-------+---------+-----+-----------+
 *
 * 32-bit length:
 * +------+-------+-------+-------+-------+---------+-----+-----------+
 * | type | len+0 | len+1 | len+2 | len+3 | value+0 | ... | value+len |
 * +------+-------+-------+-------+-------+---------+-----+-----------+
 *
 * Map:
 * 8-bit length:
 * +------+-----+-------+---------+-----+---------+-----------+
 * | type | len | key+0 | value+0 | ... | key+len | value+len |
 * +------+-----+-------+---------+-----+---------+-----------+
 *
 * 16-bit length:
 * +------+-------+-------+-------+---------+-----+---------+-----------+
 * | type | len+0 | len+1 | key+0 | value+0 | ... | key+len | value+len |
 * +------+-------+-------+-------+---------+-----+---------+-----------+
 *
 * 32-bit length:
 * +------+-------+-------+-------+-------+-------+---------+-----+---------+-----------+
 * | type | len+0 | len+1 | len+2 | len+3 | key+0 | value+0 | ... | key+len | value+len |
 * +------+-------+-------+-------+-------+-------+---------+-----+---------+-----------+
 *
 * For arrays and maps, key are always strings and values can be any packable type.
 *
 */

namespace io
{

enum io_buf_pack_type
{
    /* 0-0x7f const unsigned int 0-127 */
    /* 0x80-0x9f fixed length string, max length 32 bytes */
    /* 0xa0-0xbf fixed length binary blob, max length 32 bytes */
    /* 0xc0-0xcf fixed length array, max 16 entries*/
    /* 0xd0-0xdf fixed length map, max 16 entries*/

    IO_BUF_PACK_FIXUINT      = 0x00, /**< const unsigned int 0-127 */
    IO_BUF_PACK_FIXUINT_MAX  = 0x7f, /**< const unsigned int 0-127 */

    IO_BUF_PACK_FIXSTR       = 0x80, /**< fixed length string, max length 32 bytes */
    IO_BUF_PACK_FIXSTR_MAX   = 0x9f, /**< fixed length string, max length 32 bytes */

    IO_BUF_PACK_FIXMAP       = 0xa0, /**< fixed length map, max 32 entries*/
    IO_BUF_PACK_FIXMAP_MAX   = 0xbf, /**< fixed length map, max 32 entries*/

    IO_BUF_PACK_FIXARRAY     = 0xc0, /**< fixed length array, max 16 entries*/
    IO_BUF_PACK_FIXARRAY_MAX = 0xcf, /**< fixed length array, max 16 entries*/

    IO_BUF_PACK_FIXBIN       = 0xd0, /**< fixed length binary blob, max length 16 bytes */
    IO_BUF_PACK_FIXBIN_MAX   = 0xdf, /**< fixed length binary blob, max length 16 bytes */

    IO_BUF_PACK_NIL          = 0xe6, /**< Nil/Null */
    IO_BUF_PACK_TRUE         = 0xe7, /**< Boolean true */
    IO_BUF_PACK_FALSE        = 0xe8, /**< Boolean false */
    IO_BUF_PACK_UINT8        = 0xe9, /**< Unsigned 8-bit integer, followed by 1 byte of data */
    IO_BUF_PACK_UINT16       = 0xea, /**< Unsigned 16-bit integer, followed by 2 bytes of data */
    IO_BUF_PACK_UINT32       = 0xeb, /**< Unsigned 32-bit integer, followed by 4 bytes of data */
    IO_BUF_PACK_UINT64       = 0xec, /**< Unsigned 64-bit integer, followed by 8 bytes of data */
    IO_BUF_PACK_INT8         = 0xed, /**< Signed 8-bit integer, followed by 1 byte of data */
    IO_BUF_PACK_INT16        = 0xee, /**< Signed 16-bit integer, followed by 2 bytes of data */
    IO_BUF_PACK_INT32        = 0xef, /**< Signed 32-bit integer, followed by 4 bytes of data */
    IO_BUF_PACK_INT64        = 0xf0, /**< Signed 64-bit integer, followed by 8 bytes of data */
    IO_BUF_PACK_FLOAT        = 0xf1, /**< 32-bit floating point number, followed by 4 bytes of data */
    IO_BUF_PACK_DOUBLE       = 0xf2, /**< 64-bit floating point number, followed by 8 bytes of data */
    IO_BUF_PACK_STR8         = 0xf3, /**< String, followed by 1 byte for length, followed by the string data */
    IO_BUF_PACK_STR16        = 0xf4, /**< String, followed by 2 bytes for  length, followed by the string data */
    IO_BUF_PACK_STR32        = 0xf5, /**< String, followed by 4 bytes for  length, followed by the string data */
    IO_BUF_PACK_BIN8         = 0xf6, /**< Binary blob, followed by 1 byte for length, followed by the blob data */
    IO_BUF_PACK_BIN16        = 0xf7, /**< Binary blob, followed by 2 bytes for length, followed by the blob data */
    IO_BUF_PACK_BIN32        = 0xf8, /**< Binary blob, followed by 4 bytes for length, followed by the blob data */
    IO_BUF_PACK_ARRAY8       = 0xf9, /**< Array, followed by 1 byte for array's length, followed by the array data */
    IO_BUF_PACK_ARRAY16      = 0xfa, /**< Array, followed by 2 bytes for array's length, followed by the array data */
    IO_BUF_PACK_ARRAY32      = 0xfb, /**< Array, followed by 4 bytes for array's length, followed by the array data */
    IO_BUF_PACK_MAP8         = 0xfc, /**< Map, followed by 1 byte for map's length, followed by the map data */
    IO_BUF_PACK_MAP16        = 0xfd, /**< Map, followed by 2 bytes for map's length, followed by the map data */
    IO_BUF_PACK_MAP32        = 0xfe, /**< Map, followed by 4 bytes for map's length, followed by the map data */

    IO_BUF_PACK_MAX,                 /**< Maximum type value */
    IO_BUF_PACK_INVALID = 0xff       /**< Invalid type */
};

/**
 * @brief Theory of Operation: Sub-buffer Functionality
 *
 * The io_buf class supports creating sub-buffers that reference portions of a parent buffer.
 * This is accomplished through the constructor io_buf(io_buf &buf, off_t offset, size_t size).
 *
 * Key features of sub-buffers:
 * 1. Memory Management: Sub-buffers share the underlying memory with their parent using
 *    a shared_ptr. This means no data copying occurs when creating a sub-buffer.
 *
 * 2. Offset Tracking: Each sub-buffer maintains its own offset (_buf_offset) relative to 
 *    the parent buffer's data. The original offset is preserved in _buf_offset_orig.
 *
 * 3. Parent-Child Relationship: A reference to the parent buffer is stored in parent_.
 *    When a top-level buffer is created, it becomes its own parent.
 *
 * 4. Buffer Adjustment: The adjust_offset() method allows parent buffers to update their
 *    length based on writes to child buffers. This ensures that data written to a sub-buffer
 *    is properly reflected in the parent's readable size.
 *
 * 5. Independent Read/Write Pointers: Each sub-buffer maintains independent read and write
 *    positions, allowing different parts of a buffer to be processed concurrently.
 *
 * Example use cases:
 * - Processing different sections of a protocol message in parallel
 * - Creating a view into a specific region of a buffer
 * - Building composite data structures by assembling sub-buffers
 *
 * Note: Changes to a sub-buffer modify the same underlying memory as the parent buffer.
 * However, parent buffer length will only be extended if adjust_offset() is called.
 */
class io_buf
{
    using io_buf_container = std::shared_ptr<std::vector<char>>;

  public:
    // no empty buffer
    io_buf() = delete;

    /**
     * @brief Create a new io_buf buffer.
     *
     * @param size Size of the buffer in bytes.
     */
    io_buf(size_t size) : parent_(*this), _buf(std::make_shared<std::vector<char>>(size)), _buf_size(size)
    {
    }

    /**
     * @brief Create a new sub io_buf buffer from an existing buffer.
     *
     * This constructor creates a view into a portion of an existing buffer.
     * The created sub-buffer shares the same underlying memory as the parent.
     * 
     * @param buf The parent buffer to create a sub-buffer from
     * @param offset Offset from the start of the parent's readable data
     * @param size Maximum size of the sub-buffer
     */
    io_buf(io_buf &buf, off_t offset, size_t size)
    : parent_(buf), 
      _buf(buf._buf), 
      _buf_size(size), 
      _buf_offset(buf._buf_offset + offset),  // Add parent's offset to this buffer's offset
      _buf_offset_orig(buf._buf_offset + offset)  // Store original offset with parent's offset included
    {
        // Ensure we don't exceed the parent's buffer
        // Fix for signedness warning - cast offset to size_t for comparison
        if (offset >= 0 && static_cast<size_t>(offset) + size > buf.size()) {
            _buf_size = static_cast<size_t>(offset) < buf.size() ? 
                         buf.size() - static_cast<size_t>(offset) : 0;
        }
        else if (offset < 0) {
            // Handle negative offsets safely
            _buf_size = 0;
        }
    }

    /**
     * @brief Copy constructor.
     */
    io_buf(const io_buf &buf)
    : parent_(buf.parent_),
      _buf(buf._buf),
      _buf_size(buf._buf_size),
      _buf_len(buf._buf_len),
      _buf_offset(buf._buf_offset),
      _buf_offset_orig(buf._buf_offset_orig),
      _overrun_flag(buf._overrun_flag)
    {
    }

    /**
     * @brief Move constructor.
     */
    io_buf(io_buf &&buf)
    : parent_(buf.parent_),
      _buf(std::move(buf._buf)),
      _buf_size(buf._buf_size),
      _buf_len(buf._buf_len),
      _buf_offset(buf._buf_offset),
      _buf_offset_orig(buf._buf_offset_orig),
      _overrun_flag(buf._overrun_flag)
    {
        buf._buf = std::make_shared<std::vector<char>>(buf._buf_size);
    }

    /**
     * @brief Copy assignment operator.
     */
    io_buf &operator=(const io_buf &buf)
    {
        _buf             = buf._buf;
        _buf_size        = buf._buf_size;
        _buf_len         = buf._buf_len;
        _buf_offset      = buf._buf_offset;
        _buf_offset_orig = buf._buf_offset_orig;
        _overrun_flag    = buf._overrun_flag;
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    io_buf &operator=(io_buf &&buf)
    {
        _buf             = std::move(buf._buf);
        _buf_size        = buf._buf_size;
        _buf_len         = buf._buf_len;
        _buf_offset      = buf._buf_offset;
        _buf_offset_orig = buf._buf_offset_orig;
        _overrun_flag    = buf._overrun_flag;

        buf._buf = std::make_shared<std::vector<char>>(buf._buf_size);
        return *this;
    }

    /**
     * @brief Destructor.
     */
    ~io_buf()
    {
        if (&parent_ == this)
        {
            _buf->clear();
        }
    }

    /**
     * @brief Reset the buffer pointers.
     */
    void reset(bool clear = false);

    /**
     * @brief Resize the buffer.
     * 
     * Can only resize if we own the buffer. (i.e. not a sub-buffer)
     */
    void resize(size_t size);

    /**
     * @brief Adjust the buffer's offset based on the child buffer's offset.
     */
    void adjust_offset(const io_buf &other);

    /**
     * @brief Returns the size of the payload in the buffer.
     *
     * This is the total size payload in the buffer, from the payload pointer to the end of the buffer.
     */
    size_t size() const noexcept  // add noexcept where methods won't throw
    {
        return _buf_size;
    }

    /**
     * @brief Return the number of bytes that have been written to the payload.
     */
    auto readable() const
    {
        return _buf_len;
    }

    /**
     * @brief Returns the number of bytes that can be written to the payload.
     */
    auto writable() const
    {
        return _buf_size - _buf_len;
    }

    /**
     * @brief Returns true if any write operation has attempted to write beyond the buffer's capacity.
     * 
     * The flag is cleared when the buffer is reset.
     */
    bool overrun() const
    {
        return _overrun_flag;
    }

    /**
     * @brief Returns a pointer to the start of the data in the buffer.
     */
    char *read_ptr() const
    {
        return _buf->data() + _buf_offset;
    }

    /**
     * @brief Returns a pointer to the start of the data in the buffer with an offset.
     */
    char *read_ptr(off_t offset) const;

    /**
     * @brief Returns a pointer to the end of the data in the buffer.
     */
    char *write_ptr() const
    {
        return _buf->data() + _buf_offset + _buf_len;
    }

    /**
     * @brief Returns a pointer to the end of the data in the buffer with an offset.
     */
    char *write_ptr(off_t offset);

    /**
     * @brief Advance the write pointer by @p len bytes.
     */
    void advance_write_ptr(size_t len);

    /**
     * @brief Advance the read pointer by @p len bytes.
     */
    void advance_read_ptr(size_t len);

    off_t normalize_write_offset(off_t offset) const;

    off_t normalize_read_offset(off_t offset) const;

    size_t write(const char *data, size_t len, off_t offset = -1, bool advance = true);

    size_t read(char *data, size_t len, off_t offset = 0, bool advance = true);

    size_t read8(uint8_t &value, off_t offset = 0, bool advance = true);
    size_t read16(uint16_t &value, off_t offset = 0, bool advance = true);
    size_t read32(uint32_t &value, off_t offset = 0, bool advance = true);
    size_t read64(uint64_t &value, off_t offset = 0, bool advance = true);

    size_t write8(uint8_t value, off_t offset = -1, bool advance = true);
    size_t write16(uint16_t value, off_t offset = -1, bool advance = true);
    size_t write32(uint32_t value, off_t offset = -1, bool advance = true);
    size_t write64(uint64_t value, off_t offset = -1, bool advance = true);

    // Add string_view support for improved performance
    size_t write(std::string_view data, off_t offset = -1, bool advance = true);

    template <typename T> size_t pack_size(const T &value);
    template <typename T> size_t pack_size(const T *value);
    template <typename T> size_t pack_size(const std::vector<T> &value);
    template <typename T> size_t pack_size(const std::map<std::string, T> &value);

    template <typename T> size_t pack(const std::vector<T> &value, off_t offset = -1, bool advance = true);
    template <typename T> size_t pack(const T &value, off_t offset = -1, bool advance = true);

    template <typename T> size_t unpack(std::vector<T> &value, off_t offset = -1, bool advance = true);
    template <typename T> size_t unpack(T &value, off_t offset = 0, bool advance = true);

    template <typename T> size_t unpack_type(T &value, off_t offset = 0, bool advance = true);
    template <typename T> size_t unpack_size(T &value, off_t offset = 0, bool advance = true);

    std::string to_string() const;

  private:
    io_buf &parent_; /**< Parent io_buf if this is a sub io_buf */
    io_buf_container _buf;
    size_t _buf_size{0};       /**< Size of the buffer */
    size_t _buf_len{0};        /**< Number of bytes written to the buffer */
    off_t _buf_offset{0};      /**< Offset into the buffer. Will be 0 in the io_buf that created _buf */
    off_t _buf_offset_orig{0}; /**< offset that the buffer was created with */
    bool _overrun_flag{false}; /**< Flag indicating if a write operation has attempted to write beyond buffer capacity */
};

} // namespace io
