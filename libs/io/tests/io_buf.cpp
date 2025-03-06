#include <common/log.hpp>
#include <common/catch.hpp>
#include <io/iobuf.hpp>
#include <io/impl/iobuf.hpp>
#include <io/impl/iobuf_pack.hpp>

#include <string>
#include <list>

using namespace io;

TEST_CASE("iobuf basic operations", "[iobuf]")
{
    SECTION("create and check size") {
        io_buf buf(100);
        REQUIRE(buf.size() == 100);
        REQUIRE(buf.readable() == 0);
        REQUIRE(buf.writable() == 100);
    }

    SECTION("write and read pointers") {
        io_buf buf(100);
        char* write_ptr = buf.write_ptr();
        REQUIRE(write_ptr != nullptr);
        
        memcpy(write_ptr, "test", 4);
        buf.advance_write_ptr(4);
        
        REQUIRE(buf.readable() == 4);
        REQUIRE(buf.writable() == 96);
        
        char* read_ptr = buf.read_ptr();
        REQUIRE(read_ptr != nullptr);
        REQUIRE(strncmp(read_ptr, "test", 4) == 0);
    }

    SECTION("read pointer with offset") {
        io_buf buf(100);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        REQUIRE(strncmp(buf.read_ptr(2), "stdata", 6) == 0);
        REQUIRE(strncmp(buf.read_ptr(-3), "ata", 3) == 0);
        REQUIRE(buf.read_ptr(8) == nullptr);
        REQUIRE(buf.read_ptr(-9) == nullptr);
    }

    SECTION("buffer boundaries") {
        io_buf buf(10);
        REQUIRE(buf.writable() == 10);
        
        buf.advance_write_ptr(11); // Should not advance past buffer size
        REQUIRE(buf.readable() == 0);
        
        memcpy(buf.write_ptr(), "1234567890", 10);
        buf.advance_write_ptr(10);
        REQUIRE(buf.writable() == 0);
        
        buf.advance_read_ptr(11); // Should not advance past readable data
        REQUIRE(buf.readable() == 10);
    }
}

TEST_CASE("iobuf adjust offset", "[iobuf]") {
    SECTION("parent buffer adjusts length based on child") {
        io_buf parent(100);
        io_buf child(parent, 10, 50);
        
        // Write some data to child buffer
        memcpy(child.write_ptr(), "test", 4);
        child.advance_write_ptr(4);
        
        // Parent should adjust its length 
        parent.adjust_offset(child);
        REQUIRE(parent.readable() == 14); // child offset (10) + child length (4)
        
        // Write more to child
        memcpy(child.write_ptr(), "more", 4);
        child.advance_write_ptr(4);
        
        parent.adjust_offset(child);
        REQUIRE(parent.readable() == 18); // child offset (10) + child length (8)
    }

    SECTION("child should not adjust parent length if the parent is already longer") {
        io_buf parent(100);
        io_buf child(parent, 8, 50);
        
        // Write some data to parent
        memcpy(parent.write_ptr(), "verylongdatablahblah", 20);
        parent.advance_write_ptr(20);

        // write some data to child
        memcpy(child.write_ptr(), "child", 5);
        child.advance_write_ptr(5);
        
        // Child should not adjust parent length
        child.adjust_offset(parent);
        REQUIRE(parent.readable() == 20);
        REQUIRE(child.readable() == 5);


        // check the parent buffer it should contain child at offset 8
        REQUIRE(strncmp(parent.read_ptr(8), "child", 5) == 0);

    }

    SECTION("child buffer adjust_offset has no effect") {
        io_buf parent(100);
        io_buf child(parent, 10, 50);
        
        // Write to parent
        memcpy(parent.write_ptr(), "parentdata", 10);
        parent.advance_write_ptr(10);
        
        // Child adjust_offset should have no effect
        child.adjust_offset(parent);
        REQUIRE(parent.readable() == 10);
        REQUIRE(child.readable() == 0);
    }
}

TEST_CASE("iobuf write operations", "[iobuf]") {
    SECTION("write with default offset") {
        io_buf buf(100);
        REQUIRE(buf.write("test", 4) == 4);
        REQUIRE(buf.readable() == 4);
        REQUIRE(strncmp(buf.read_ptr(), "test", 4) == 0);

        REQUIRE(buf.write("more", 4) == 4);
        REQUIRE(buf.readable() == 8);
        REQUIRE(strncmp(buf.read_ptr(), "testmore", 8) == 0);
    }

    SECTION("write with positive offset") {
        io_buf buf(100);
        REQUIRE(buf.write("initial", 7) == 7);
        REQUIRE(buf.write("test", 4, 2) == 4);
        REQUIRE(buf.readable() == 7);
        REQUIRE(strncmp(buf.read_ptr(), "intest", 6) == 0);
    }

    SECTION("write with negative offset") {
        io_buf buf(100);
        REQUIRE(buf.write("hello", 5) == 5);
        REQUIRE(buf.write("xyz", 3, -2) == 3);
        REQUIRE(strncmp(buf.read_ptr(), "hellxyz", 7) == 0);
        REQUIRE(buf.readable() == 7);
    }

    SECTION("write without advance") {
        io_buf buf(100);
        REQUIRE(buf.write("test", 4) == 4);
        REQUIRE(buf.write("xyz", 3, 1, false) == 3);
        REQUIRE(buf.readable() == 4);
        REQUIRE(strncmp(buf.read_ptr(), "txyz", 4) == 0);
    }

    SECTION("write boundary conditions") {
        io_buf buf(10);
        
        // Write beyond buffer size
        REQUIRE(buf.write("toolongstring", 12) == 10);
        
        // Write with offset beyond buffer size
        REQUIRE(buf.write("test", 4, 11) == 0);
        
        // Write with negative offset beyond data
        REQUIRE(buf.write("abc", 3) == 0); // at end of buffer
        REQUIRE(buf.write("test", 4, -5) == 4);
        
        // Write that would overflow from offset
        REQUIRE(buf.write("overflow", 8, 5) == 5);
    }
}



TEST_CASE("iobuf read operations", "[iobuf]") {
    SECTION("read with default offset") {
        io_buf buf(100);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        char data[9] = {0};
        REQUIRE(buf.read(data, 4) == 4);
        REQUIRE(strncmp(data, "test", 4) == 0);
        REQUIRE(buf.readable() == 4); // 4 bytes should be consumed
    }

    SECTION("read with positive offset") {
        io_buf buf(100);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        char data[9] = {0};
        REQUIRE(buf.read(data, 4, 2) == 4);
        REQUIRE(strncmp(data, "stda", 4) == 0);
        REQUIRE(buf.readable() == 8); // No bytes should be consumed
    }

    SECTION("read with negative offset") {
        io_buf buf(100);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        char data[9] = {0};
        REQUIRE(buf.read(data, 4, -4) == 4);
        REQUIRE(strncmp(data, "data", 4) == 0);
        REQUIRE(buf.readable() == 8); // No bytes should be consumed
    }

    SECTION("read with advance") {
        io_buf buf(100);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        char data[9] = {0};
        REQUIRE(buf.read(data, 4, 2, true) == 4);
        REQUIRE(strncmp(data, "stda", 4) == 0);
        REQUIRE(buf.readable() == 8); // no bytes should be consumed because offset is not 0
    }

    SECTION("read boundary conditions") {
        io_buf buf(10);
        memcpy(buf.write_ptr(), "testdata", 8);
        buf.advance_write_ptr(8);

        char data[11] = {0};


        // Read with offset beyond buffer length
        REQUIRE(buf.read(data, 4, 9) == 0);

        // Read with negative offset beyond data
        REQUIRE(buf.read(data, 4, -9) == 0);

        // Read that would overflow from offset. Would not advance the read pointer since offset is not 0
        REQUIRE(buf.read(data, 8, 5) == 3); // 3 bytes should be read

        // Read beyond buffer length
        REQUIRE(buf.read(data, 12) == 8); // 8 bytes should be read

        // Nothing should be left to read
        REQUIRE(buf.readable() == 0);
    }
}
TEST_CASE("iobuf read/write integrals", "[iobuf]") {
    SECTION("write8/read8") {
        io_buf buf(10);
        REQUIRE(buf.write8(0x7F) == 1);

        uint8_t val8;
        REQUIRE(buf.read8(val8) == 1);
        REQUIRE(val8 == 0x7F);
        REQUIRE(buf.readable() == 0);
    }

    SECTION("write8/read8 with offset") {
        io_buf buf(10);
        REQUIRE(buf.write8(0x11) == 1);
        REQUIRE(buf.write8(0x22) == 1);
        REQUIRE(buf.write8(0x33) == 1);

        uint8_t val8 = 0;
        REQUIRE(buf.read8(val8, 1, false) == 1);
        REQUIRE(val8 == 0x22);
        REQUIRE(buf.readable() == 3);
    }

    SECTION("write16/read16") {
        io_buf buf(10);
        REQUIRE(buf.write16(0x1234) == 2);

        uint16_t val16;
        REQUIRE(buf.read16(val16) == 2);
        REQUIRE(val16 == 0x1234);
        REQUIRE(buf.readable() == 0);
    }

    SECTION("write16/read16 with offset") {
        io_buf buf(10);
        REQUIRE(buf.write16(0xAAAA) == 2);
        REQUIRE(buf.write16(0xBBBB) == 2);

        uint16_t val16 = 0;
        REQUIRE(buf.read16(val16, 2, false) == 2);
        REQUIRE(val16 == 0xBBBB);
        REQUIRE(buf.readable() == 4);
    }

    SECTION("write32/read32") {
        io_buf buf(10);
        REQUIRE(buf.write32(0xDEADBEEF) == 4);

        uint32_t val32;
        REQUIRE(buf.read32(val32) == 4);
        REQUIRE(val32 == 0xDEADBEEF);
        REQUIRE(buf.readable() == 0);
    }

    SECTION("write64/read64") {
        io_buf buf(16);
        REQUIRE(buf.write64(0x1122334455667788ULL) == 8);

        uint64_t val64;
        REQUIRE(buf.read64(val64) == 8);
        REQUIRE(val64 == 0x1122334455667788ULL);
        REQUIRE(buf.readable() == 0);
    }
}


TEST_CASE("iobuf pack pack_size", "[iobuf]") {
    SECTION("int") {
        io_buf buf(10);
        int64_t val = 0x7F;

        REQUIRE(buf.pack_size(val) == 1);
        REQUIRE(buf.pack_size(123) == 1);

        val = -0x7F;
        REQUIRE(buf.pack_size(val) == 2);
        REQUIRE(buf.pack_size(-123) == 2);

        val = 0x7FFF;
        REQUIRE(buf.pack_size(val) == 3);
        REQUIRE(buf.pack_size(12345) == 3);

        val = 0x1234;
        REQUIRE(buf.pack_size(val) == 3);
        REQUIRE(buf.pack_size(0x1234) == 3);

        val = -0x7FFF;
        REQUIRE(buf.pack_size(val) == 3);
        REQUIRE(buf.pack_size(-12345) == 3);

        val = 0x7FFFFFFF;
        REQUIRE(buf.pack_size(val) == 5);
        REQUIRE(buf.pack_size(123456789) == 5);

        val = -0x7FFFFFFF;
        REQUIRE(buf.pack_size(val) == 5);
        REQUIRE(buf.pack_size(-123456789) == 5);

        val = 0x7FFFFFFFFFFFFFFFL;
        REQUIRE(buf.pack_size(val) == 9);
    }

    SECTION("uint") {
        io_buf buf(10);
        uint64_t val = 0x7F;

        REQUIRE(buf.pack_size(val) == 1);
        REQUIRE(buf.pack_size(123UL) == 1);

        val = 0x7FFF;
        REQUIRE(buf.pack_size(val) == 3);
        REQUIRE(buf.pack_size(12345UL) == 3);

        val = 0x1234;
        REQUIRE(buf.pack_size(val) == 3);
        REQUIRE(buf.pack_size(0x1234UL) == 3);

        val = 0x7FFFFFFF;
        REQUIRE(buf.pack_size(val) == 5);
        REQUIRE(buf.pack_size(123456789UL) == 5);

        val = 0x7FFFFFFFFFFFFFFFUL;
        REQUIRE(buf.pack_size(val) == 9);
        REQUIRE(buf.pack_size(1234567890123456789UL) == 9);
    }

    SECTION("string") {
        io_buf buf(10);
        REQUIRE(buf.pack_size("test") == 5);
        REQUIRE(buf.pack_size("longstring") == 11);
    }

    SECTION("long string") {
        io_buf buf(256);
        std::string str(256, 'a');
        REQUIRE(buf.pack_size(str) == 259);
    }

    SECTION("r<uint8_t> as binary bloc") {
        io_buf buf(10);
        std::vector<uint8_t> vec = {1, 2, 3, 4, 5};
        REQUIRE(buf.pack_size(vec) == 6);

        vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
        REQUIRE(buf.pack_size(vec) == 11);
    }

    SECTION("vector<uint8_t> as very large binary bloc") {
        io_buf buf(10);
        std::vector<uint8_t> vec(256, 0);
        // generate some random data


        REQUIRE(buf.pack_size(vec) == 259);
    }

    SECTION("vector<char> as binary bloc") {
        io_buf buf(10);
        std::vector<char> vec = {1, 2, 3, 4, 5};
        REQUIRE(buf.pack_size(vec) == 6);

        vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
        REQUIRE(buf.pack_size(vec) == 11);
    }

    SECTION("float") {
        io_buf buf(10);
        REQUIRE(buf.pack_size(3.141f) == 5);
    }

    SECTION("double") {
        io_buf buf(10);
        REQUIRE(buf.pack_size(3.141) == 9);
    }

    SECTION("vector") {
        io_buf buf(10);
        std::vector<int> vec = {1, 2, 3, 4, 5};
        REQUIRE(buf.pack_size(vec) == 6);

        std::vector<std::string> vec_str = {"test", "more", "data"};
        REQUIRE(buf.pack_size(vec_str) == 16);

        std::vector<std::vector<int>> vec_vec = {{1, 2, 3}, {4, 5, 6}};
        REQUIRE(buf.pack_size(vec_vec) == 9);
        vec_vec = {
            {1, 2, 3},
            {4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        };
        CHECK(buf.pack_size(vec_vec) == 22);
    }

    SECTION("bool") {
        io_buf buf(10);
        REQUIRE(buf.pack_size(true) == 1);
        REQUIRE(buf.pack_size(false) == 1);
    }

    SECTION("NULL") {
        io_buf buf(10);
        REQUIRE(buf.pack_size(nullptr) == 1);
    }
}

// Add a new test case for buffer overflow detection
TEST_CASE("iobuf memory safety", "[io_buffer]")
{
    // Create a small buffer to test boundary conditions
    io_buf buf(8);
    
    SECTION("Writing within bounds succeeds") {
        REQUIRE(buf.write8(0x42, 0, true) == 1);
        REQUIRE(buf.write8(0x43, 1, true) == 1);
        
        uint8_t value = 0;
        REQUIRE(buf.read8(value, 0, false) == 1);
        REQUIRE(value == 0x42);
    }
    
    SECTION("Writing beyond bounds fails gracefully") {
        // Try to write beyond buffer end
        REQUIRE(buf.write8(0x42, 8, true) == 0);
        
        // Try to write uint16 at the edge
        REQUIRE(buf.write16(0x4243, 7, true) == 0);
        
        // Valid write at edge - 1
        REQUIRE(buf.write8(0x44, 7, true) == 1);
    }
    
    SECTION("Reading beyond bounds fails gracefully") {
        // Set up buffer with 4 bytes of data
        buf.write32(0x01020304, 0, true);
        
        uint8_t byte_val = 0;
        uint16_t short_val = 0;
        uint32_t int_val = 0;
        
        // Read at valid position
        REQUIRE(buf.read8(byte_val, 0, false) == 1);
        
        // Read uint16 at edge should fail
        REQUIRE(buf.read16(short_val, 3, false) == 0);
        
        // Read uint32 beyond should fail
        REQUIRE(buf.read32(int_val, 5, false) == 0);
    }
    
    SECTION("Multiple operations maintain correct state") {
        // Write and advance pointer
        REQUIRE(buf.write16(0x1234, 0, true) == 2);
        REQUIRE(buf.write16(0x5678, -1, true) == 2);
        
        // Buffer should now have 4 bytes
        REQUIRE(buf.readable() == 4);
        REQUIRE(buf.writable() == 4);
        
        // Read first value
        uint16_t val1 = 0, val2 = 0;
        REQUIRE(buf.read16(val1, 0, true) == 2);
        REQUIRE(val1 == 0x1234);
        
        // Buffer should now have 2 bytes
        REQUIRE(buf.readable() == 2);
        
        // Read second value
        REQUIRE(buf.read16(val2, 0, true) == 2);
        REQUIRE(val2 == 0x5678);
        
        // Buffer should now be empty
        REQUIRE(buf.readable() == 0);
        REQUIRE(buf.writable() == 8);
    }
}

// Add tests for the new overrun flag functionality
TEST_CASE("iobuf overrun flag", "[iobuf]") {
    SECTION("write8 sets overrun flag") {
        io_buf buf(10);
        // Fill up the buffer
        for (int i = 0; i < 10; i++) {
            REQUIRE(buf.write8(0x42 + i) == 1);
        }
        // This write should fail and set the overrun flag
        REQUIRE(buf.write8(0x4C) == 0);
        REQUIRE(buf.overrun() == true);
        
        // Reset should clear the flag
        buf.reset();
        REQUIRE(buf.overrun() == false);
    }

    SECTION("write16 sets overrun flag") {
        io_buf buf(9); // Use a 9-byte buffer instead of 10
        // Fill most of the buffer
        REQUIRE(buf.write16(0x1234) == 2);
        REQUIRE(buf.write16(0x5678) == 2);
        REQUIRE(buf.write16(0x9ABC) == 2);
        REQUIRE(buf.write16(0xDEF0) == 2); // This will fill to offset 8
        // This write should attempt to write beyond the buffer limit and set the flag
        REQUIRE(buf.write16(0x1234) == 0); // Should fail since we need 2 bytes but only have 1
        REQUIRE(buf.overrun() == true);
    }

    SECTION("write32 sets overrun flag") {
        io_buf buf(10);
        REQUIRE(buf.write32(0x12345678) == 4);
        REQUIRE(buf.write32(0x9ABCDEF0) == 4);
        // This should attempt to write 4 bytes but only 2 are available
        REQUIRE(buf.write32(0x12345678) == 0);
        REQUIRE(buf.overrun() == true);
    }

    SECTION("write64 sets overrun flag") {
        io_buf buf(10);
        // This just fits
        REQUIRE(buf.write64(0x123456789ABCDEF0ULL) == 8);
        // This won't fit
        REQUIRE(buf.write64(0x123456789ABCDEF0ULL) == 0);
        REQUIRE(buf.overrun() == true);
    }

    SECTION("write with offset sets overrun flag") {
        io_buf buf(10);
        // Write beyond available space
        REQUIRE(buf.write("test", 4, 7) == 3); // Only 3 bytes should fit
        REQUIRE(buf.overrun() == true);
    }

    SECTION("reset clears overrun flag") {
        io_buf buf(5);
        // Cause an overrun
        REQUIRE(buf.write("toolong", 7) == 5); // Only 5 bytes should fit
        REQUIRE(buf.overrun() == true);
        
        // Reset should clear the flag
        buf.reset();
        REQUIRE(buf.overrun() == false);
    }
}

// Add tests for the new to_string method
TEST_CASE("iobuf to_string formatting", "[iobuf]") {
    SECTION("buffer with printable characters") {
        io_buf buf(100);
        
        // Fill with printable ASCII
        const char* text = "Hello, world! This is a test string with printable ASCII characters.";
        buf.write(text, strlen(text));
        
        std::string formatted = buf.to_string();
        
        // Check the basic formatting elements
        REQUIRE(formatted.find("len: ") != std::string::npos);
        REQUIRE(formatted.find("00000000") != std::string::npos);  // Should have an offset
        REQUIRE(formatted.find("|Hello, world!") != std::string::npos);  // Should contain the text in ASCII section
        
        // Make sure it doesn't have the trailing length number (the bug we fixed)
        std::string last_line = formatted.substr(formatted.find_last_of('\n') + 1);
        REQUIRE(last_line.empty());
    }
    
    SECTION("buffer with non-printable characters") {
        io_buf buf(100);
        
        // Write some non-printable bytes
        uint8_t data[] = {0x00, 0x01, 0x02, 0x7F, 0x80, 0xFF, 'A', 'B', 'C'};
        buf.write(reinterpret_cast<const char*>(data), sizeof(data));
        
        std::string formatted = buf.to_string();
        
        LOG(debug) << "Formatted buffer: " << formatted.c_str();

        // Non-printable characters should be replaced with dots in the ASCII section
        REQUIRE(formatted.find("|......ABC|") != std::string::npos);
        
        // Check hex formatting for some specific bytes
        REQUIRE(formatted.find("00 01 02") != std::string::npos);
        REQUIRE(formatted.find("7f 80 ff") != std::string::npos);
        REQUIRE(formatted.find("41 42  43") != std::string::npos);  // ASCII for "ABC"
    }
    
    SECTION("empty buffer") {
        io_buf buf(100);
        
        std::string formatted = buf.to_string();
        
        // Should just include the length header
        REQUIRE(formatted == "len: 0\n");
    }
    
    SECTION("line deduplication with repeating data") {
        io_buf buf(100);
        
        // Write 3 lines of identical data
        for (size_t i = 0; i < 4 * io::detail::BYTES_PER_LINE; ++i) {
            buf.write8(0xAA);
        }
        
        std::string formatted = buf.to_string();
        
        LOG(debug) << "Formatted buffer: " << formatted.c_str();

        // Should show the first line of data, then a "*" for the duplicate lines
        REQUIRE(formatted.find("00000000  aa aa aa aa") != std::string::npos);
        REQUIRE(formatted.find("*") != std::string::npos);
        
        // Should NOT have the trailing length number at the end
        std::string last_line = formatted.substr(formatted.find_last_of('\n') + 1);
        REQUIRE(last_line.empty());
    }
    
    SECTION("formatting with exact line size") {
        io_buf buf(io::detail::BYTES_PER_LINE);
        
        // Fill exactly one line
        for (size_t i = 0; i < io::detail::BYTES_PER_LINE; ++i) {
            buf.write8(i);
        }
        
        std::string formatted = buf.to_string();
        
        // Should have one offset and one data line
        REQUIRE(formatted.find("00000000") != std::string::npos);
        
        // Check the formatting when we have a full line
        std::string expected_hex_start = "00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f";
        REQUIRE(formatted.find(expected_hex_start) != std::string::npos);
    }
    
    SECTION("formatting with partial line") {
        io_buf buf(10);  // Smaller than BYTES_PER_LINE
        
        // Fill the buffer
        for (int i = 0; i < 10; ++i) {
            buf.write8(i);
        }
        
        std::string formatted = buf.to_string();
        
        // Check that partial line is padded correctly
        REQUIRE(formatted.find("00 01 02 03 04 05 06 07  08 09") != std::string::npos);
        
        // Should have spaces instead of data for the remaining columns
        REQUIRE(formatted.find("00 01 02 03 04 05 06 07  08 09                    |") != std::string::npos);
    }

    SECTION("alternating duplicate and non-duplicate lines") {
        io_buf buf(200);
        
        // First fill with repeating pattern (32 bytes = 2 lines)
        for (int i = 0; i < 32; i++) {
            buf.write8(0xAA);
        }
        
        // Add a different line
        for (int i = 0; i < 16; i++) {
            buf.write8(0xBB);
        }
        
        // Add repeating pattern again (32 bytes = 2 lines)
        for (int i = 0; i < 32; i++) {
            buf.write8(0xAA);
        }
        
        // Add another different line at the end
        for (int i = 0; i < 16; i++) {
            buf.write8(0xCC);
        }
        
        std::string formatted = buf.to_string();
        
        // Check for expected pattern in output
        // First we should see one line of AA values
        REQUIRE(formatted.find("00000000  aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa") != std::string::npos);
        // Then should see a * for the duplicate AA line
        REQUIRE(formatted.find("*") != std::string::npos);
        // Then should see the BB line
        REQUIRE(formatted.find("bb bb bb bb bb bb bb bb  bb bb bb bb bb bb bb bb") != std::string::npos);
        // Then another AA line
        REQUIRE(formatted.find("00000030  aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa") != std::string::npos);
        // Then another * for the duplicate AA line
        size_t first_star = formatted.find("*");
        REQUIRE(formatted.find("*", first_star + 1) != std::string::npos);
        // Then the CC line
        REQUIRE(formatted.find("cc cc cc cc cc cc cc cc  cc cc cc cc cc cc cc cc") != std::string::npos);
    }
}

// Add tests for the string_view write operation
TEST_CASE("iobuf string_view operations", "[iobuf]") {
    SECTION("write with string_view") {
        io_buf buf(100);
        
        std::string_view sv = "Hello, string_view!";
        REQUIRE(buf.write(sv) == sv.size());
        
        REQUIRE(buf.readable() == sv.size());
        REQUIRE(strncmp(buf.read_ptr(), sv.data(), sv.size()) == 0);
    }
    
    SECTION("write string_view with offset") {
        io_buf buf(100);
        buf.write("XXXXX", 5);
        
        std::string_view sv = "Hello";
        REQUIRE(buf.write(sv, 2) == sv.size());
        
        REQUIRE(buf.readable() == 7);
        REQUIRE(strncmp(buf.read_ptr(), "XXHello", 7) == 0);
    }
    
    SECTION("performance comparison with c-string") {
        io_buf buf(1000);
        std::string large_string(500, 'X');
        
        // This is not a real performance test, just validating both approaches work
        REQUIRE(buf.write(large_string.c_str(), large_string.size()) == large_string.size());
        buf.reset();
        REQUIRE(buf.write(std::string_view(large_string)) == large_string.size());
    }
}

// Add tests for resize behavior
TEST_CASE("iobuf resize operations", "[iobuf]") {
    SECTION("resize increases capacity") {
        io_buf buf(10);
        REQUIRE(buf.size() == 10);
        
        buf.write("1234567890", 10);
        REQUIRE(buf.writable() == 0);
        
        buf.resize(20);
        REQUIRE(buf.size() == 20);
        REQUIRE(buf.readable() == 10);
        REQUIRE(buf.writable() == 10);
        
        // Verify original data is intact
        REQUIRE(strncmp(buf.read_ptr(), "1234567890", 10) == 0);
        
        // Write additional data
        REQUIRE(buf.write("ABCDEFGHIJ", 10) == 10);
        REQUIRE(buf.readable() == 20);
        REQUIRE(strncmp(buf.read_ptr() + 10, "ABCDEFGHIJ", 10) == 0);
    }
    
    SECTION("resize with active data preservation") {
        io_buf buf(10);
        buf.write("ABCDE", 5);
        
        // Advance read pointer to simulate consumed data
        char temp[3];
        buf.read(temp, 3);
        
        // Before resize: 3 bytes consumed, 2 bytes remaining, 8 bytes free
        REQUIRE(buf.readable() == 2);
        REQUIRE(buf.writable() == 8);
        
        buf.resize(15);
        
        // After resize: still 2 bytes readable, but more writable space
        REQUIRE(buf.readable() == 2);
        REQUIRE(buf.writable() == 13);
        REQUIRE(strncmp(buf.read_ptr(), "DE", 2) == 0);
    }
    
    SECTION("sub-buffer cannot be resized") {
        io_buf parent(10);
        io_buf child(parent, 2, 5);
        
        size_t original_size = child.size();
        child.resize(20);  // This should have no effect
        
        REQUIRE(child.size() == original_size);  // Size should remain unchanged
    }
}

// Test advanced sub-buffer operations
TEST_CASE("iobuf advanced sub-buffer operations", "[iobuf]") {
    
    SECTION("sub-buffer at edge of parent") {
        io_buf parent(50);
        parent.write("parent data", 11);
        
        // Create sub-buffer at the end of parent's data
        io_buf child(parent, 11, 20);
        child.write("appended", 8);
        parent.adjust_offset(child);
        
        // Total length should include both segments
        REQUIRE(parent.readable() == 19);
        REQUIRE(strncmp(parent.read_ptr(), "parent dataappended", 19) == 0);
    }
    
    SECTION("overlapping writes between parent and child") {
        io_buf parent(50);
        parent.write("XXXXXXXXXXXX", 12);
        
        // Create sub-buffer overlapping parent's data
        io_buf child(parent, 6, 20);
        
        // Modify part of parent's data through child
        child.write("OOOO", 4);
        
        // Verify change is visible in parent
        REQUIRE(strncmp(parent.read_ptr(), "XXXXXXOOOOXX", 12) == 0);
    }
}

// Test edge cases with binary data
TEST_CASE("iobuf binary data handling", "[iobuf]") {
    SECTION("write/read binary data with zeros") {
        io_buf buf(100);
        
        // Create data with embedded zeros
        unsigned char binary_data[] = {0x48, 0x00, 0x65, 0x00, 0x6C, 0x6C, 0x00, 0x6F};
        buf.write(reinterpret_cast<const char*>(binary_data), sizeof(binary_data));
        
        // Read back and verify
        unsigned char read_data[sizeof(binary_data)];
        REQUIRE(buf.read(reinterpret_cast<char*>(read_data), sizeof(binary_data)) == sizeof(binary_data));
        
        for (size_t i = 0; i < sizeof(binary_data); i++) {
            REQUIRE(read_data[i] == binary_data[i]);
        }
    }
    
    SECTION("to_string with binary data") {
        io_buf buf(32);
        
        // Mix of printable and non-printable bytes
        unsigned char mixed_data[] = {
            0x41, 0x42, 0x00, 0x01, 0x7F, 0xFF, 0x43, 0x44,
            0x20, 0x09, 0x0A, 0x0D, 0x45, 0x46, 0x7E, 0x5C
        };
        
        buf.write(reinterpret_cast<const char*>(mixed_data), sizeof(mixed_data));
        
        std::string formatted = buf.to_string();
        
        LOG(debug) << "Formatted buffer: " << formatted.c_str();
        // Check that ASCII part correctly shows printable characters
        // Update the expected format to match the actual output format with space before the pipe
        REQUIRE(formatted.find(" |AB....CD ...EF~\\|") != std::string::npos);
    }
}

// Test concurrent operations on different parts of the same buffer
TEST_CASE("iobuf concurrent operations", "[iobuf]") {
    SECTION("multiple views of the same buffer") {
        io_buf main_buf(100);
        
        // Fill the buffer with placeholder data
        for (int i = 0; i < 100; i++) {
            main_buf.write8(i);
        }
        
        // Create multiple sub-buffers for different logical sections
        io_buf header(main_buf, 0, 20);
        io_buf body(main_buf, 20, 60);
        io_buf footer(main_buf, 80, 20);
        
        // Modify each section independently
        header.write("HEADER", 6, 0, true);
        body.write("BODY", 4, 0, true);
        footer.write("FOOTER", 6, 0, true);
        
        // Verify changes in the main buffer
        REQUIRE(strncmp(main_buf.read_ptr(0), "HEADER", 6) == 0);
        REQUIRE(strncmp(main_buf.read_ptr(20), "BODY", 4) == 0);
        REQUIRE(strncmp(main_buf.read_ptr(80), "FOOTER", 6) == 0);
    }
}

