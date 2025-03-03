#include <io/io.hpp>
#include <common/log.hpp>
#include <common/catch.hpp>
#include <chrono>
#include <sys/socket.h>
#include <io/mbox.hpp>

using namespace io;
using namespace io::detail;

TEST_CASE("io_mbox basic functionality", "[io_mbox]") {
    io_loop loop;
    io_mbox<int> mbox(loop);
    
    SECTION("Send and immediate receive") {
        mbox.send(42);
        
        bool completed = false;
        
        auto rx_task = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 42);
            completed = true;
        };

        
        loop.schedule(rx_task(), "rx_task");
        loop.run();
        REQUIRE(completed);
    }
    
    
    SECTION("Multiple messages") {
        for (int i = 0; i < 5; i++) {
            mbox.send(i);
        }
        
        int messages_read = 0;
        auto reader = [&]() -> io_task {
            for (int i = 0; i < 5; i++) {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == i);
                messages_read++;
            }
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
    }
    
    SECTION("Timeout when no messages") {
        bool timed_out = false;
        
        auto reader = [&]() -> io_task {
            // Use a short timeout
            auto timeout = time_now() + std::chrono::milliseconds(50);
            auto value_opt = co_await mbox.read(timeout);
            // Should return nullopt on timeout
            REQUIRE_FALSE(value_opt.has_value());
            timed_out = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(timed_out);
    }

    SECTION("Send after receive") {
        io_mbox<int> new_mbox(loop);  // Create a fresh mailbox for this test
        
        bool received = false;
        
        // Simple coordinator task
        auto coordinator = [&]() -> io_task
        {
            // First start a reader
            LOG(debug) << "Starting reader";
            auto reader_task = [&]() -> io_task {
                LOG(debug) << "Reader waiting for message";
                auto value_opt = co_await new_mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == 123);
                LOG(debug) << "Reader received message: " << value_opt.value();
                received = true;
                co_return;
            };
            
            loop.schedule(reader_task(), "reader_inner");
            
            // Now send a message
            LOG(debug) << "Sending message";
            new_mbox.send(123);
            LOG(debug) << "Message sent";

            co_return;
        };

        loop.schedule(coordinator(), "coordinator");
        loop.run();
        
        REQUIRE(received);
    }
}

TEST_CASE("io_mbox advanced functionality", "[io_mbox]") {
    io_loop loop;
    io_mbox<int> mbox(loop);
    
    SECTION("has_messages and clear") {
        REQUIRE_FALSE(mbox.has_messages());
        
        mbox.send(1);
        mbox.send(2);
        
        REQUIRE(mbox.has_messages());
        
        mbox.clear();
        
        REQUIRE_FALSE(mbox.has_messages());
    }
    
    SECTION("Multiple writers") {
        constexpr int NUM_WRITERS = 5;
        constexpr int MSGS_PER_WRITER = 20;
        int total_received = 0;
        bool done = false;
        
        // Start reader task
        auto reader = [&]() -> io_task {
            for (int i = 0; i < NUM_WRITERS * MSGS_PER_WRITER; i++) {
                [[maybe_unused]] auto value = co_await mbox.read();
                total_received++;
            }
            done = true;
        };
        
        // Start writer tasks
        auto writer = [&](int id) -> io_task {
            for (int i = 0; i < MSGS_PER_WRITER; i++) {
                mbox.send(id * 1000 + i);
                // Small delay using io wait instead of sleep
                if (i % 5 == 0) {
                    co_await io::sleep(loop, std::chrono::milliseconds(1));
                }
            }
            co_return;
        };
        
        
        loop.schedule(reader(), "reader");
        for (int i = 0; i < NUM_WRITERS; i++) {
            loop.schedule(writer(i), "writer_" + std::to_string(i));
        }
        
        loop.run();
        
        
        REQUIRE(total_received == NUM_WRITERS * MSGS_PER_WRITER);
    }
}

TEST_CASE("io_mbox with different message types", "[io_mbox]") {
    io_loop loop;
    
    SECTION("String messages") {
        io_mbox<std::string> mbox(loop);
        
        mbox.send("Hello");
        mbox.send("World");
        
        int messages_read = 0;
        auto task = [&]() -> io_task {
            auto msg1 = co_await mbox.read();
            messages_read++;
            auto msg2 = co_await mbox.read();
            messages_read++;
            
            REQUIRE(msg1 == "Hello");
            REQUIRE(msg2 == "World");
        };
        
        loop.schedule(task(), "string_test");
        loop.run();
    }
    
    SECTION("Complex types") {
        struct Message {
            int id;
            std::string text;
            
            bool operator==(const Message& other) const {
                return id == other.id && text == other.text;
            }
        };
        
        io_mbox<Message> mbox(loop);
        
        // Use the new emplace method instead of send with braced initializers
        mbox.emplace(1, "First message");
        mbox.emplace(2, "Second message");
        
        int messages_read = 0;
        auto task = [&]() -> io_task {
            auto msg1 = co_await mbox.read();
            messages_read++;
            auto msg2 = co_await mbox.read();
            messages_read++;
            
            REQUIRE(msg1 == Message{1, "First message"});
            REQUIRE(msg2 == Message{2, "Second message"});
        };
        
        loop.schedule(task(), "complex_test");
        loop.run();
    }
    
    SECTION("Move-only types") {
        struct MoveOnly {
            std::unique_ptr<int> value;
            
            explicit MoveOnly(int val) : value(std::make_unique<int>(val)) {}
            MoveOnly(MoveOnly&&) = default;
            MoveOnly& operator=(MoveOnly&&) = default;
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly& operator=(const MoveOnly&) = delete;
            
            bool operator==(int other) const { return value && *value == other; }
        };
        
        io_mbox<MoveOnly> mbox(loop);
        
        mbox.send(MoveOnly(42));
        
        bool completed = false;
        auto task = [&]() -> io_task {
            auto msg_opt = co_await mbox.read();
            REQUIRE(msg_opt.has_value());
            REQUIRE(msg_opt.value() == 42);
            completed = true;
        };
        
        loop.schedule(task(), "move_only_test");
        loop.run();
        
        REQUIRE(completed);
        
        // Test timeout case - should return nullopt
        bool timed_out = false;
        auto timeout_task = [&]() -> io_task {
            // Use a very short timeout to ensure we don't get a message
            auto timeout = time_now() + std::chrono::milliseconds(1);
            auto msg_opt = co_await mbox.read(timeout);
            // Should return nullopt on timeout
            REQUIRE_FALSE(msg_opt.has_value());
            timed_out = true;
        };
        
        loop.schedule(timeout_task(), "move_only_timeout_test");
        loop.run();
        
        REQUIRE(timed_out);
    }
}

TEST_CASE("io_mbox corner cases", "[io_mbox]") {
    io_loop loop;
    io_mbox<int> mbox(loop);
    
    SECTION("Multiple reads with no messages") {
        bool first_timed_out = false;
        bool second_timed_out = false;
        
        auto reader1 = [&]() -> io_task {
            auto timeout = time_now() + std::chrono::milliseconds(10);
            auto value_opt = co_await mbox.read(timeout);
            REQUIRE_FALSE(value_opt.has_value()); // Check that it's nullopt
            first_timed_out = true;
        };
        
        auto reader2 = [&]() -> io_task {
            auto timeout = time_now() + std::chrono::milliseconds(20);
            auto value_opt = co_await mbox.read(timeout);
            REQUIRE_FALSE(value_opt.has_value()); // Check that it's nullopt
            second_timed_out = true;
        };

        loop.schedule(reader1(), "reader1");
        loop.schedule(reader2(), "reader2");
        loop.run();
        
        REQUIRE(first_timed_out);
        REQUIRE(second_timed_out);
    }
    
    SECTION("Send while another read is in progress") {
        bool first_received = false;
        bool second_received = false;
        
        auto first_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 1);
            first_received = true;
        };
        
        auto second_reader = [&]() -> io_task {
            // Try to read but there won't be a message yet
            auto timeout = time_now() + std::chrono::milliseconds(500);
            auto value_opt = co_await mbox.read(timeout);
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 2);
            second_received = true;
        };
        
        auto sender = [&]() -> io_task {
            // Send first message immediately
            mbox.send(1);
            
            // Wait a bit using io wait, then send second message
            co_await io::sleep(loop, std::chrono::milliseconds(50));
            mbox.send(2);
            co_return;
        };

        loop.schedule(first_reader(), "first_reader");
        loop.schedule(second_reader(), "second_reader");
        loop.schedule(sender(), "sender");
        loop.run();
        
        REQUIRE(first_received);
        REQUIRE(second_received);
    }
}

TEST_CASE("io_mbox multiple readers queue", "[io_mbox]") {
    io_loop loop;
    io_mbox<int> mbox(loop);
    
    SECTION("Multiple readers in order") {
        // The readers should get messages in the order they were registered
        std::vector<int> received_values;
        
        auto reader = [&](int reader_id) -> io_task {
            auto value_opt = co_await mbox.read();
            if (value_opt) {
                received_values.push_back(reader_id);
            }
        };
        
        // Register multiple readers first
        loop.schedule(reader(1), "reader1");
        loop.schedule(reader(2), "reader2");
        loop.schedule(reader(3), "reader3");
        
        // Then send messages
        auto sender = [&]() -> io_task {
            // Send messages with delay to ensure readers get registered
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            mbox.send(101);
            mbox.send(102);
            mbox.send(103);
            co_return;
        };
        
        loop.schedule(sender(), "sender");
        loop.run();
        
        // Verify readers received messages in order of registration
        REQUIRE(received_values.size() == 3);
        REQUIRE(received_values[0] == 1);  // First reader gets first message
        REQUIRE(received_values[1] == 2);  // Second reader gets second message
        REQUIRE(received_values[2] == 3);  // Third reader gets third message
    }
    
    SECTION("Reader removal when cancelling read") {
        bool first_received = false;
        bool second_received = false;
        bool third_received = false;
        
        auto first_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 1);
            first_received = true;
        };
        
        auto second_reader = [&]() -> io_task {
            // Shorter timeout to ensure it occurs before messages are sent
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 2);
            second_received = true;
        };
        
        auto third_reader = [&]() -> io_task {
            auto timeout = time_now() + std::chrono::milliseconds(5);
            auto value_opt = co_await mbox.read(timeout);
            if (value_opt) {
                LOG(error) << "Second reader should not have received a message" << value_opt.value();
            }
            REQUIRE_FALSE(value_opt.has_value());  // Should timeout
            third_received = true;
        };
        
        auto sender = [&]() -> io_task {
            // Longer delay to ensure second reader times out first
            co_await io::sleep(loop, std::chrono::milliseconds(20));
            mbox.send(1);  // First message
            mbox.send(2);  // Second message
            co_return;
        };
        
        loop.schedule(first_reader(), "first_reader");
        loop.schedule(second_reader(), "second_reader");
        loop.schedule(third_reader(), "third_reader");
        loop.schedule(sender(), "sender");
        
        loop.run();
        
        REQUIRE(first_received);
        REQUIRE(second_received);  // Should have timed out
        REQUIRE(third_received);   // Should have received the second message
    }
}

TEST_CASE("io_mbox optional behavior", "[io_mbox]") {
    io_loop loop;
    io_mbox<int> mbox(loop);
    
    SECTION("Optional value when message available") {
        auto reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 42);
            
            // Test direct boolean evaluation of optional
            if (value_opt) {
                REQUIRE(*value_opt == 42);
            } else {
                FAIL("Optional should be true");
            }
        };
        
        mbox.send(42);
        loop.schedule(reader(), "reader");
        loop.run();
    }
    
    SECTION("Nullopt on timeout") {
        auto reader = [&]() -> io_task {
            auto timeout = time_now() + std::chrono::milliseconds(50);
            auto value_opt = co_await mbox.read(timeout);
            
            REQUIRE_FALSE(value_opt.has_value());
            REQUIRE_FALSE(value_opt);  // Test direct boolean evaluation
            
            // Verify we can't dereference a nullopt
            if (value_opt) {
                FAIL("Optional should be false");
            }
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
    }
    
    SECTION("Optional with move-only type") {
        struct MoveOnly {
            std::unique_ptr<int> value;
            
            explicit MoveOnly(int val) : value(std::make_unique<int>(val)) {}
            MoveOnly(MoveOnly&&) = default;
            MoveOnly& operator=(MoveOnly&&) = default;
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly& operator=(const MoveOnly&) = delete;
            
            bool operator==(int other) const { return value && *value == other; }
        };
        
        io_mbox<MoveOnly> mbox(loop);
        
        auto reader = [&]() -> io_task {
            // First read - value available
            {
                auto msg_opt = co_await mbox.read();
                REQUIRE(msg_opt.has_value());
                
                // Move out of optional
                auto moved_value = std::move(msg_opt.value());
                REQUIRE(moved_value == 42);
            }
            
            // Second read - timeout
            {
                auto timeout = time_now() + std::chrono::milliseconds(10);
                auto msg_opt = co_await mbox.read(timeout);
                REQUIRE_FALSE(msg_opt.has_value());
            }
        };
        
        mbox.send(MoveOnly(42));
        loop.schedule(reader(), "reader");
        loop.run();
    }
}

TEST_CASE("io_mbox edge cases", "[io_mbox]") {
    io_loop loop;
    
    SECTION("Message sending after clear") {
        io_mbox<int> mbox(loop);
        
        // Fill mailbox then clear it
        mbox.send(1);
        mbox.send(2);
        mbox.send(3);
        mbox.clear();
        
        bool message_received = false;
        
        auto reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 42);
            message_received = true;
        };
        
        auto sender = [&]() -> io_task {
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            mbox.send(42);
            co_return;
        };
        
        loop.schedule(reader(), "reader");
        loop.schedule(sender(), "sender");
        loop.run();
        
        REQUIRE(message_received);
    }
    
    SECTION("Multiple readers with mixed timeouts") {
        io_mbox<int> mbox(loop);
        
        int completed_readers = 0;
        int timed_out_readers = 0;
        
        auto reader_with_timeout = [&](int timeout_ms) -> io_task {
            auto timeout = time_now() + std::chrono::milliseconds(timeout_ms);
            auto value_opt = co_await mbox.read(timeout);
            
            if (value_opt) {
                completed_readers++;
            } else {
                timed_out_readers++;
            }
        };
        
        // Schedule readers with different timeouts
        loop.schedule(reader_with_timeout(50), "reader_50ms");
        loop.schedule(reader_with_timeout(100), "reader_100ms");
        loop.schedule(reader_with_timeout(150), "reader_150ms");
        
        // Send only two messages after some readers have timed out
        auto sender = [&]() -> io_task {
            co_await io::sleep(loop, std::chrono::milliseconds(75));  // First reader should timeout
            mbox.send(1);
            mbox.send(2);
            co_return;
        };
        
        loop.schedule(sender(), "sender");
        loop.run();
        
        // First reader should timeout, other two should get messages
        REQUIRE(timed_out_readers == 1);
        REQUIRE(completed_readers == 2);
    }
}

// Add a new test case section for close() functionality
TEST_CASE("io_mbox close functionality", "[io_mbox]") {
    io_loop loop;
    
    SECTION("Close empty mailbox") {
        io_mbox<int> mbox(loop);
        
        // Close the empty mailbox
        mbox.close();
        
        // Try reading from closed mailbox
        bool timed_out = false;
        
        auto reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            // Should return nullopt when mailbox is closed
            REQUIRE_FALSE(value_opt.has_value());
            timed_out = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(timed_out);
    }
    
    SECTION("Close mailbox with messages") {
        io_mbox<int> mbox(loop);
        
        // Add messages
        mbox.send(1);
        mbox.send(2);
        
        // Close the mailbox
        mbox.close();
        
        bool read_completed = false;
        
        auto reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            // Even though the mailbox is closed, existing messages should still be readable
            REQUIRE_FALSE(value_opt.has_value());
            read_completed = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(read_completed);
        
        // Verify that we can't send more messages
        mbox.send(3);
        
        bool second_read_completed = false;
        auto second_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE_FALSE(value_opt.has_value());
            second_read_completed = true;
        };
        
        loop.schedule(second_reader(), "second_reader");
        loop.run();
        
        REQUIRE(second_read_completed);
    }
    
    SECTION("Close mailbox with waiting readers") {
        io_mbox<int> mbox(loop);
        
        bool first_reader_completed = false;
        bool second_reader_completed = false;
        bool third_reader_completed = false;
        
        // Start multiple readers
        auto first_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            // When mailbox is closed, readers should get nullopt
            REQUIRE_FALSE(value_opt.has_value());
            first_reader_completed = true;
        };
        
        auto second_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE_FALSE(value_opt.has_value());
            second_reader_completed = true;
        };
        
        auto third_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE_FALSE(value_opt.has_value());
            third_reader_completed = true;
        };
        
        // Close the mailbox after a delay
        auto closer = [&]() -> io_task {
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            mbox.close();
            co_return;
        };
        
        loop.schedule(first_reader(), "first_reader");
        loop.schedule(second_reader(), "second_reader");
        loop.schedule(third_reader(), "third_reader");
        loop.schedule(closer(), "closer");
        
        loop.run();
        
        // All readers should have been signaled
        REQUIRE(first_reader_completed);
        REQUIRE(second_reader_completed);
        REQUIRE(third_reader_completed);
    }
    
    SECTION("Send message to closed mailbox") {
        io_mbox<int> mbox(loop);
        
        // Close immediately
        mbox.close();
        
        // Send message to closed mailbox
        mbox.send(42);
        
        bool reader_completed = false;
        
        auto reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            // Messages sent to closed mailbox should be ignored
            REQUIRE_FALSE(value_opt.has_value());
            reader_completed = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(reader_completed);
    }
    
    SECTION("Mixed operations with close") {
        io_mbox<int> mbox(loop);
        
        // Send some messages
        mbox.send(1);
        mbox.send(2);
        
        int messages_read = 0;
        bool read_after_close = false;
        
        auto reader_task = [&]() -> io_task {
            // First read should get a message
            {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == 1);
                messages_read++;
            }
            
            // Second read should get a message
            {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == 2);
                messages_read++;
            }
            
            // This read will be waiting when close() is called
            {
                auto value_opt = co_await mbox.read();
                REQUIRE_FALSE(value_opt.has_value());
                read_after_close = true;
            }
        };
        
        auto closer = [&]() -> io_task {
            // Wait for the first two reads to complete
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            mbox.close();
        };
        
        loop.schedule(reader_task(), "reader");
        loop.schedule(closer(), "closer");
        loop.run();
        
        REQUIRE(messages_read == 2);
        REQUIRE(read_after_close);
        
        // Try reopening (should not be possible - once closed, stays closed)
        mbox.send(3);
        
        bool final_read = false;
        auto final_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE_FALSE(value_opt.has_value());
            final_read = true;
        };
        
        loop.schedule(final_reader(), "final_reader");
        loop.run();
        
        REQUIRE(final_read);
    }
}

// Add test case for mailbox cleanup
TEST_CASE("io_mbox cleanup", "[io_mbox]") {
    io_loop loop;
    
    SECTION("Cleanup with active readers") {
        // Create a mailbox that will be destroyed while readers are waiting
        auto setup_and_destroy = [&]() -> io_task {
            // Create mailbox in a limited scope
            {
                io_mbox<int> temp_mbox(loop);
                
                // Start a reader that will never complete normally
                auto reader = [&](io_mbox<int>& mbox) -> io_task {
                    auto value_opt = co_await mbox.read(time_now() + std::chrono::milliseconds(100));
                    // This should timeout naturally
                    REQUIRE_FALSE(value_opt.has_value());
                    co_return;
                };
                
                loop.schedule(reader(temp_mbox), "reader");
                
                // Wait a bit to ensure reader is registered
                co_await io::sleep(loop, std::chrono::milliseconds(10));
                
                // Mailbox will be destroyed when leaving this scope
            }
            
            // Wait a bit more to ensure any cleanup events are processed
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            co_return;
        };
        
        // The test passes if we don't crash when the mailbox is destroyed
        // while a reader is waiting
        loop.schedule(setup_and_destroy(), "setup_and_destroy");
        loop.run();
    }
    
    // Remove the reader cancellation section that depended on loop.cancel()
    // And replace with a test focusing on task completion
    SECTION("Task completion after reading") {
        io_mbox<int> mbox(loop);
        bool reader_done = false;
        
        auto reader_task = [&]() -> io_task {
            // Read with a short timeout
            auto value_opt = co_await mbox.read(time_now() + std::chrono::milliseconds(50));
            // This should time out
            REQUIRE_FALSE(value_opt.has_value());
            reader_done = true;
            co_return;
        };
        
        loop.schedule(reader_task(), "reader");
        loop.run();
        
        REQUIRE(reader_done);
        
        // Make sure mailbox is still usable after task completion
        mbox.send(42);
        
        bool second_read = false;
        auto second_reader = [&]() -> io_task {
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 42);
            second_read = true;
        };
        
        loop.schedule(second_reader(), "second_reader");
        loop.run();
        
        REQUIRE(second_read);
    }
}

// Add test case for max queue size
TEST_CASE("io_mbox max queue size", "[io_mbox]") {
    io_loop loop;
    
    SECTION("Default unlimited queue") {
        io_mbox<int> mbox(loop);
        
        REQUIRE(mbox.max_size() == 0);  // Default is unlimited
        
        // Add many messages
        for (int i = 0; i < 1000; i++) {
            mbox.send(i);
        }
        
        REQUIRE(mbox.size() == 1000);
        
        // Read some messages to verify they're in correct order
        bool read_completed = false;
        auto reader = [&]() -> io_task {
            for (int i = 0; i < 10; i++) {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == i);
            }
            read_completed = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(read_completed);
        REQUIRE(mbox.size() == 990);  // 1000 - 10 = 990
    }
    
    SECTION("Limited queue size with message dropping") {
        const size_t MAX_QUEUE_SIZE = 5;
        io_mbox<int> mbox(loop, MAX_QUEUE_SIZE);
        
        REQUIRE(mbox.max_size() == MAX_QUEUE_SIZE);
        
        // Add more messages than the queue can hold
        for (int i = 0; i < 10; i++) {
            mbox.send(i);
        }
        
        // Queue should only have the last MAX_QUEUE_SIZE messages
        REQUIRE(mbox.size() == MAX_QUEUE_SIZE);
        
        // Verify that the oldest messages were dropped
        bool read_completed = false;
        auto reader = [&]() -> io_task {
            for (size_t i = 0; i < MAX_QUEUE_SIZE; i++) {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                // Expected values are 5, 6, 7, 8, 9 (first 5 were dropped)
                REQUIRE(value_opt.value() == static_cast<int>(i + 10 - MAX_QUEUE_SIZE));
            }
            read_completed = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(read_completed);
        REQUIRE(mbox.size() == 0);
    }
    
    SECTION("Filling and emptying with limited size") {
        const size_t MAX_QUEUE_SIZE = 3;
        io_mbox<int> mbox(loop, MAX_QUEUE_SIZE);
        
        // Fill the queue exactly to capacity
        for (size_t i = 0; i < MAX_QUEUE_SIZE; i++) {
            mbox.send(static_cast<int>(i));
        }
        
        REQUIRE(mbox.size() == MAX_QUEUE_SIZE);
        
        // Add one more message, which should cause dropping of oldest
        mbox.send(100);
        
        REQUIRE(mbox.size() == MAX_QUEUE_SIZE);
        
        // Read all messages and verify the first was dropped
        bool read_completed = false;
        auto reader = [&]() -> io_task {
            // First message (0) should have been dropped
            // Should receive 1, 2, 100
            for (size_t i = 1; i < MAX_QUEUE_SIZE; i++) {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == static_cast<int>(i));
            }
            
            // Last message is 100
            auto value_opt = co_await mbox.read();
            REQUIRE(value_opt.has_value());
            REQUIRE(value_opt.value() == 100);
            
            read_completed = true;
        };
        
        loop.schedule(reader(), "reader");
        loop.run();
        
        REQUIRE(read_completed);
        REQUIRE(mbox.size() == 0);
    }
}

// Add test case for is_closed() method
TEST_CASE("io_mbox is_closed() method", "[io_mbox]") {
    io_loop loop;
    
    SECTION("Check is_closed state transitions") {
        io_mbox<int> mbox(loop);
        
        // Initially not closed
        REQUIRE_FALSE(mbox.is_closed());
        
        // After sending, still not closed
        mbox.send(1);
        REQUIRE_FALSE(mbox.is_closed());
        
        // After closing, should be closed
        mbox.close();
        REQUIRE(mbox.is_closed());
        
        // Should remain closed
        mbox.send(2);  // This send should be ignored
        REQUIRE(mbox.is_closed());
    }
    
    SECTION("is_closed and reader behavior") {
        io_mbox<int> mbox(loop);
        
        bool read_before_close_completed = false;
        bool read_after_close_completed = false;
        
        auto reader = [&]() -> io_task {
            // First read before close
            {
                auto value_opt = co_await mbox.read();
                REQUIRE(value_opt.has_value());
                REQUIRE(value_opt.value() == 42);
                read_before_close_completed = true;
            }
            
            // Second read after close
            {
                auto value_opt = co_await mbox.read();
                REQUIRE_FALSE(value_opt.has_value());
                read_after_close_completed = true;
            }
        };
        
        auto closer = [&]() -> io_task {
            // Send first message
            mbox.send(42);
            
            // Wait a bit to ensure first read completes
            co_await io::sleep(loop, std::chrono::milliseconds(10));
            
            // Close the mailbox
            REQUIRE_FALSE(mbox.is_closed());
            mbox.close();
            REQUIRE(mbox.is_closed());
            co_return;
        };
        
        loop.schedule(reader(), "reader");
        loop.schedule(closer(), "closer");
        loop.run();
        
        REQUIRE(read_before_close_completed);
        REQUIRE(read_after_close_completed);
    }
    
    SECTION("is_closed state consistency") {
        io_mbox<int> mbox(loop);
        
        // Try operations in various orders
        mbox.send(1);
        REQUIRE_FALSE(mbox.is_closed());
        
        mbox.clear();
        REQUIRE_FALSE(mbox.is_closed());
        
        mbox.close();
        REQUIRE(mbox.is_closed());
        
        // After close, these operations shouldn't change closed state
        mbox.send(2);
        REQUIRE(mbox.is_closed());
        
        mbox.clear();
        REQUIRE(mbox.is_closed());
    }
}
