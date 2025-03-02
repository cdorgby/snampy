#include <io/iotask.hpp>
#include <common/catch.hpp>
#include <chrono>
#include <thread>

using namespace io;

// Helper functions for testing
io_func<int> simple_coroutine()
{
    co_return 42;
}

io_func<int> chained_coroutine()
{
    int value = co_await simple_coroutine();
    co_return value + 1;
}

io_func<void> void_coroutine()
{
    co_return;
}

io_func<int> throwing_coroutine()
{
    throw std::runtime_error("test error");
    co_return 0;
}

io_func<int> long_running_coroutine()
{
    for (int i = 0; i < 1000; i++) {
        co_await void_coroutine();  // simulate work
    }
    co_return 100;
}

// Helper class for reference testing
class RefTestObject {
public:
    RefTestObject() : value_(0) {}
    explicit RefTestObject(int v) : value_(v) {}
    int value() const { return value_; }
    void set_value(int v) { value_ = v; }
private:
    int value_;
};

// Helper coroutines for reference testing
io_func<RefTestObject&> return_ref(RefTestObject& obj)
{
    co_return obj;
}

io_func<const RefTestObject&> return_const_ref(const RefTestObject& obj)
{
    co_return obj;
}

io_func<RefTestObject&> modify_and_return_ref(RefTestObject& obj)
{
    obj.set_value(obj.value() + 1);
    co_return obj;
}

io_func<RefTestObject&> chain_ref(RefTestObject& obj)
{
    RefTestObject& ref = co_await return_ref(obj);  // Explicit type annotation
    ref.set_value(ref.value() + 1);
    co_return ref;
}

io_func<const std::vector<int>&> return_local_const_vector_ref()
{
    static std::vector<int> local_vec{1, 2, 3};  // Make static to ensure lifetime
    co_return local_vec;
}

TEST_CASE("io_func basic functionality", "[io_task]")
{
    SECTION("Simple coroutine returns expected value")
    {
        auto task = simple_coroutine();
        REQUIRE(task.handle() != nullptr);
        task.handle().resume();
        REQUIRE(task.handle().done());
        REQUIRE(task.handle().promise().get_result() == 42);
    }

    SECTION("Void coroutine completes successfully")
    {
        auto task = void_coroutine();
        REQUIRE(task.handle() != nullptr);
        task.handle().resume();
        REQUIRE(task.handle().done());
        REQUIRE_NOTHROW(task.handle().promise().get_result());
    }
}

TEST_CASE("io_func chaining", "[io_task]")
{
    SECTION("Chained coroutines work correctly")
    {
        auto task = chained_coroutine();
        REQUIRE(task.handle() != nullptr);
        
        // Resume until completion
        while (!task.handle().done()) {
            task.handle().resume();
        }
        
        REQUIRE(task.handle().promise().get_result() == 43);
    }
}

TEST_CASE("io_func error handling", "[io_task]")
{
    SECTION("Exceptions are propagated correctly")
    {
        auto task = throwing_coroutine();
        REQUIRE(task.handle() != nullptr);
        task.handle().resume();
        REQUIRE(task.handle().done());
        REQUIRE_THROWS_AS(task.handle().promise().get_result(), std::runtime_error);
    }
}

TEST_CASE("io_func move semantics", "[io_task]")
{
    SECTION("Move construction works correctly")
    {
        auto task1 = simple_coroutine();
        auto handle = task1.handle();
        auto task2 = std::move(task1);
        
        REQUIRE(task1.handle() == nullptr);
        REQUIRE(task2.handle() == handle);
    }

    SECTION("Move assignment works correctly")
    {
        auto task1 = simple_coroutine();
        auto task2 = simple_coroutine();
        auto handle = task1.handle();
        
        task2 = std::move(task1);
        REQUIRE(task1.handle() == nullptr);
        REQUIRE(task2.handle() == handle);
    }
}

TEST_CASE("io_func is_ready", "[io_task]")
{
    SECTION("is_ready returns correct value")
    {
        auto task = simple_coroutine();
        REQUIRE_FALSE(task.is_ready());
        task.handle().resume();
        REQUIRE(task.is_ready());
    }
}

TEST_CASE("io_func exception propagation", "[io_task]")
{
    SECTION("Exceptions are propagated correctly")
    {
        auto task = throwing_coroutine();
        REQUIRE(task.handle() != nullptr);
        task.handle().resume();
        REQUIRE(task.handle().done());
        REQUIRE_THROWS_AS(task.result(), std::runtime_error);
    }
}

TEST_CASE("io_func awaiting completed task", "[io_task]")
{
    SECTION("Awaiting a completed task returns immediately")
    {
        auto task = simple_coroutine();
        task.handle().resume();
        REQUIRE(task.is_ready());

        auto awaiter = [](io_func<int>& completed_task) -> io_func<int> {
            auto result = co_await completed_task;
            co_return result;
        }(task);

        awaiter.handle().resume();
        REQUIRE(awaiter.is_ready());
        REQUIRE(awaiter.result() == 42);
    }
}

TEST_CASE("io_func synchronous completion", "[io_task]")
{
    SECTION("Task completes synchronously")
    {
        auto task = []() -> io_func<int> {
            co_return 123;
        }();

        task.handle().resume();
        REQUIRE(task.is_ready());
        REQUIRE(task.result() == 123);
    }
}

TEST_CASE("io_func multiple awaits", "[io_task]")
{
    SECTION("Awaiting the same task multiple times")
    {
        auto task = simple_coroutine();
        while (!task.is_ready()) {
            task.handle().resume();
        }
        
        // Store awaiters in variables to control their lifetime
        auto awaiter1 = [](io_func<int>& completed_task) -> io_func<int> {
            auto result = co_await completed_task;
            co_return result;
        }(task);
        
        awaiter1.handle().resume();
        REQUIRE(awaiter1.is_ready());
        REQUIRE(awaiter1.result() == 42);
        
        auto awaiter2 = [](io_func<int>& completed_task) -> io_func<int> {
            auto result = co_await completed_task;
            co_return result;
        }(task);
        
        awaiter2.handle().resume();
        REQUIRE(awaiter2.is_ready());
        REQUIRE(awaiter2.result() == 42);
    }
}

TEST_CASE("io_func reference return", "[io_task]")
{
    SECTION("Basic reference return")
    {
        RefTestObject obj(42);
        auto task = return_ref(obj);
        task.handle().resume();
        REQUIRE(task.is_ready());
        
        RefTestObject& result = task.result();
        REQUIRE(&result == &obj);  // Check it's the same object
        REQUIRE(result.value() == 42);
    }

    SECTION("Const reference return")
    {
        RefTestObject obj(42);
        auto task = return_const_ref(obj);
        task.handle().resume();
        REQUIRE(task.is_ready());
        
        const RefTestObject& result = task.result();
        REQUIRE(&result == &obj);  // Check it's the same object
        REQUIRE(result.value() == 42);
    }

    SECTION("Modify through reference")
    {
        RefTestObject obj(42);
        auto task = modify_and_return_ref(obj);
        task.handle().resume();
        REQUIRE(task.is_ready());
        
        RefTestObject& result = task.result();
        REQUIRE(&result == &obj);  // Check it's the same object
        REQUIRE(result.value() == 43);  // Value should be incremented
        REQUIRE(obj.value() == 43);     // Original object should be modified
    }

    SECTION("Chained reference operations")
    {
        RefTestObject obj(42);
        auto task = chain_ref(obj);
        
        while (!task.is_ready()) {
            task.handle().resume();
        }
        
        RefTestObject& result = task.result();
        REQUIRE(&result == &obj);  // Check it's the same object
        REQUIRE(result.value() == 43);  // Value should be incremented
        REQUIRE(obj.value() == 43);     // Original object should be modified
    }

    SECTION("Return local const vector reference")
    {
        auto task = return_local_const_vector_ref();
        task.handle().resume();
        REQUIRE(task.is_ready());

        const std::vector<int>& result = task.result();
        REQUIRE(result == std::vector<int>({1, 2, 3}));
    }
}

TEST_CASE("io_func task ID", "[io_task]")
{
    SECTION("Task ID is set correctly")
    {
        auto task = simple_coroutine();
        REQUIRE(task.task_id().rfind("#", 0) == 0); // Starts with '#'
    }

    SECTION("Task ID can be set manually")
    {
        auto task = simple_coroutine();
        task.set_task_id("custom_id");
        REQUIRE(task.task_id() == "custom_id");
    }
}
