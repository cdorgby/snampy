#include <io/io.hpp>
#include <common/log.hpp>
#include <common/catch.hpp>
#include <chrono>
#include <sys/socket.h>
#include <fcntl.h>  // Add missing header for fcntl
#include <thread>
#include <atomic>
#include <future>

using namespace io;
using namespace io::detail;


TEST_CASE("Concurrent promise completion", "[io_loop]") {
    io_loop_basic<epoll_poller> loop;
    loop.init();
    
    const int NUM_PROMISES = 100;
    std::vector<std::unique_ptr<io_awaitable>> promises;
    std::atomic<int> completed_count{0};
    
    for (int i = 0; i < NUM_PROMISES; i++) {
        promises.push_back(std::make_unique<io_awaitable>(loop, time_now() + std::chrono::milliseconds(500)));
    }
    
    auto waiter_task = [&]() -> io_task {
        std::vector<io_awaitable*> raw_promises;
        for (auto& p : promises) {
            raw_promises.push_back(p.get());
        }
        
        // Wait for all promises
        auto ready = co_await io_wait_for_all_promise{loop, time_now() + std::chrono::seconds(2), raw_promises};
        
        // Count completed promises
        completed_count = ready.size();
        co_return;
    };
    
    auto completion_task = [&]() -> io_task {
        // Small initial delay
        auto delay = io_awaitable{loop, time_now() + std::chrono::milliseconds(50)};
        co_await delay;
        
        // Complete all promises nearly simultaneously
        for (auto& p : promises) {
            p->waiter_.complete(io_result::done);
        }
        
        co_return;
    };
    
    loop.schedule(waiter_task(), "waiter");
    loop.schedule(completion_task(), "completer");
    loop.run();
    
    // All promises should have completed
    REQUIRE(completed_count == NUM_PROMISES);
}

TEST_CASE("Multiple loop interaction", "[io_loop]") {
    // Create two separate loops
    io_loop_basic<epoll_poller> loop1;
    io_loop_basic<epoll_poller> loop2;
    loop1.init();
    loop2.init();
    
    // Create a promise on each loop
    auto p1 = io_awaitable{loop1, time_now() + std::chrono::milliseconds(100)};
    auto p2 = io_awaitable{loop2, time_now() + std::chrono::milliseconds(100)};
    
    std::atomic<bool> loop1_done{false};
    std::atomic<bool> loop2_done{false};
    
    // Task for loop1
    auto task1 = [&]() -> io_task {
        auto result = co_await p1;
        REQUIRE(result == io_result::timeout);
        loop1_done = true;
        co_return;
    };
    
    // Task for loop2
    auto task2 = [&]() -> io_task {
        auto result = co_await p2;
        REQUIRE(result == io_result::timeout);
        loop2_done = true;
        co_return;
    };
    
    // Schedule tasks on their respective loops
    loop1.schedule(task1(), "loop1_task");
    loop2.schedule(task2(), "loop2_task");
    
    // Run both loops in separate threads
    auto f1 = std::async(std::launch::async, [&]() { loop1.run(); });
    auto f2 = std::async(std::launch::async, [&]() { loop2.run(); });
    
    // Wait for both loops to complete
    f1.get();
    f2.get();
    
    REQUIRE(loop1_done);
    REQUIRE(loop2_done);
}

TEST_CASE("Extremely short timeouts", "[io_loop]") {
    io_loop_basic<epoll_poller> loop;
    loop.init();
    
    // Promise with extremely short timeout (0ms)
    auto p = io_awaitable{loop, time_now()};
    
    bool task_completed = false;
    auto task = [&]() -> io_task {
        auto result = co_await p;
        REQUIRE(result == io_result::timeout);
        REQUIRE(p.timeout());
        task_completed = true;
        co_return;
    };
    
    loop.schedule(task(), "short_timeout");
    loop.run();
    
    REQUIRE(task_completed);
}

TEST_CASE("Error handling for complex operation chains", "[io_loop]") {
    io_loop_basic<epoll_poller> loop;
    loop.init();
    
    std::vector<std::error_code> propagated_errors;
    
    auto leaf_task = [&]() -> io_task {
        // Simulate a system error
        auto p = io_awaitable{loop, time_now() + std::chrono::seconds(1)};
        errno = ECONNREFUSED;
        p.waiter_.awaitable_->set_error(std::error_code(errno, std::system_category()));
        p.waiter_.complete(io_result::error);
        
        auto result = co_await p;
        REQUIRE(result == io_result::error);
        REQUIRE(p.error().value() == ECONNREFUSED);
        
        propagated_errors.push_back(p.error());
        co_return;
    };
    
    auto middle_task = [&]() -> io_task {
        try {
            co_await leaf_task();
        } catch (...) {
            FAIL("Should not throw exception");
        }
        
        // Add a custom error
        propagated_errors.push_back(make_error_code(io_errc::operation_timeout));
        co_return;
    };
    
    auto root_task = [&]() -> io_task {
        co_await middle_task();
        
        // Verify that we can continue after an error
        auto p = io_awaitable{loop, time_now() + std::chrono::milliseconds(10)};
        auto result = co_await p;
        REQUIRE(result == io_result::timeout);
        
        // Add final error
        propagated_errors.push_back(p.error());
        co_return;
    };
    
    loop.schedule(root_task(), "error_chain");
    loop.run();
    
    // Verify all expected errors were collected
    REQUIRE(propagated_errors.size() == 3);
    REQUIRE(propagated_errors[0].value() == ECONNREFUSED);
    REQUIRE(propagated_errors[1].value() == static_cast<int>(io_errc::operation_timeout));
    REQUIRE(propagated_errors[2].value() == static_cast<int>(io_errc::operation_timeout));
}

TEST_CASE("Resource cleanup after cancellation", "[io_loop]") {
    io_loop_basic<epoll_poller> loop;
    loop.init();
    
    // Create a pipe for testing
    int pipefd[2];
    REQUIRE(pipe(pipefd) == 0);
    
    auto cleanup_task = [&]() -> io_task {
        // Create a promise for the read end
        auto read_promise = io_desc_awaitable{loop, pipefd[0], io_desc_type::read, 
                                          time_now() + std::chrono::milliseconds(500)};
        
        // Start waiting for data
        auto read_task = [&]() -> io_task {
            auto result = co_await read_promise;
            
            // Should be cancelled, not completed normally
            REQUIRE(result == io_result::cancelled);
            REQUIRE(read_promise.canceled());
            
            co_return;
        };
        
        auto t = read_task();
        
        // Wait briefly then cancel the read
        auto delay = io_awaitable{loop, time_now() + std::chrono::milliseconds(50)};
        co_await delay;
        
        read_promise.cancel();
        
        // Wait for read task to complete
        co_await t;
        
        // Verify the fd is still valid and wasn't closed by the cancellation
        REQUIRE(fcntl(pipefd[0], F_GETFD) != -1);
        
        // Clean up manually
        close(pipefd[0]);
        close(pipefd[1]);
        
        co_return;
    };
    
    loop.schedule(cleanup_task(), "cleanup_task");
    loop.run();
}

// This tests that waiter reset/reuse functionality works correctly
TEST_CASE("Waiter reuse with complex operations", "[io_loop]") {
    io_loop_basic<epoll_poller> loop;
    loop.init();
    
    auto p = io_awaitable{loop, time_now() + std::chrono::milliseconds(100)};
    int completion_count = 0;
    
    auto test_task = [&]() -> io_task {
        // First usage
        auto result = co_await p;
        REQUIRE(result == io_result::timeout);
        completion_count++;
        
        // Reset and reuse
        p.reset(time_now() + std::chrono::milliseconds(100));
        result = co_await p;
        REQUIRE(result == io_result::timeout);
        completion_count++;
        
        // Reset and cancel immediately
        p.reset(time_now() + std::chrono::seconds(10));
        p.cancel();
        result = co_await p;
        REQUIRE(result == io_result::cancelled);
        completion_count++;
        
        // Reset and use with io_wait_for_any
        p.reset(time_now() + std::chrono::milliseconds(100));
        auto p2 = io_awaitable{loop, time_now() + std::chrono::milliseconds(200)};
        
        auto ready = co_await io_wait_for_any{loop, time_now() + std::chrono::milliseconds(150), {&p, &p2}};
        REQUIRE(ready.size() == 1);
        REQUIRE(ready[0] == &p);
        
        result = co_await p;
        REQUIRE(result == io_result::timeout);
        completion_count++;
        
        // Clean up p2
        result = co_await p2;
        REQUIRE(result == io_result::timeout);
        
        co_return;
    };
    
    loop.schedule(test_task(), "reuse_test");
    loop.run();
    
    REQUIRE(completion_count == 4);
}
