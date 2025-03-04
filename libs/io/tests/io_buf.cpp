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

