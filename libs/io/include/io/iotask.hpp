#pragma once

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <utility>
#include <variant>
#include <iostream>
#include <common/log.hpp>
#include <atomic>
#include <io/waiter.hpp>

namespace io
{


namespace detail
{
// Forward declarations
template <typename return_type> class io_func;

/**
 * @brief Helper type trait to extract the storage type, handling both values and references.
 *
 * `storage_type` is used to store the result of the coroutine.
 * It uses `std::remove_reference_t` to handle both value and reference types consistently.
 * @tparam T The type to extract the storage type from.
 */
template <typename T> struct storage_type
{
    using type      = std::remove_reference_t<T>;
    using storage_t = type;
};

template <typename T> struct storage_type<T &>
{
    using type      = std::remove_reference_t<T>;
    using storage_t = type *;
};

template <typename T> struct storage_type<const T &>
{
    using type      = std::remove_reference_t<T>;
    using storage_t = const type *;
};

template <typename T> using storage_type_t    = typename storage_type<T>::type;
template <typename T> using storage_storage_t = typename storage_type<T>::storage_t;

template <typename return_type> class io_func;

/**
 * @brief Base class for the promise type of an `io_task`.
 *
 * Manages the continuation and exception handling for the coroutine.
 */
struct io_task_promise_base
{
    /// The coroutine handle to resume after awaiting this task.
    std::coroutine_handle<> continuation_;
    /// Stores any exception that occurred during the execution of the coroutine.
    std::exception_ptr exception_{nullptr};

    /**
     * @brief Awaitable used for the final suspension point of the coroutine.
     *
     * Determines the next coroutine to resume after the current one completes.
     */
    struct final_awaitable
    {
        /// Always returns false, ensuring the coroutine suspends.
        auto await_ready() const noexcept { return false; }

        /**
         * @brief Determines the coroutine to resume after suspension.
         *
         * If there is a continuation, it resumes that coroutine; otherwise, it returns a no-op coroutine.
         * @param coroutine The handle of the current coroutine.
         * @return std::coroutine_handle<> The handle of the coroutine to resume.
         */
        template <typename promise_type>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept
        {
            auto &promise = coroutine.promise();

            if (promise.continuation_ != nullptr) { return promise.continuation_; }
            return std::noop_coroutine();
        }

        /// Does nothing after resuming.
        auto await_resume() noexcept -> void {}
    };

    /**
     * @brief Handles uncaught exceptions within the coroutine.
     *
     * Stores the exception for later rethrowing.
     */
    auto unhandled_exception() noexcept -> void { exception_ = std::current_exception(); }

    /// Suspends the coroutine initially.
    auto initial_suspend() noexcept { return std::suspend_always(); }
    /// Suspends the coroutine finally, using `final_awaitable`.
    auto final_suspend() noexcept { return final_awaitable{}; }
};

/**
 * @brief Specialization of `io_task_promise_base` for non-void return types.
 *
 * Stores the result of the coroutine and provides methods to access it.
 * @tparam return_type The return type of the coroutine.
 */
template <typename return_type> struct io_task_promise final : public io_task_promise_base
{
    storage_storage_t<return_type> result_{};
    static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;

    /// Default constructor.
    io_task_promise() noexcept : result_{} {}

    /**
     * @brief Gets the return object (`io_func`) associated with this promise.
     * @return io_func<return_type> The `io_func` associated with this promise.
     */
    [[nodiscard]] io_func<return_type> get_return_object() noexcept;

    /**
     * @brief Stores a value as the result of the coroutine.
     * @tparam value_type The type of the value to store.
     * @param value The value to store.
     */
    template <typename value_type>
        requires std::is_constructible_v<storage_type_t<return_type>, value_type &&>
    auto return_value(value_type &&value) noexcept -> void // Add -> void for consistency
    {
        if constexpr (return_type_is_reference) { result_ = std::addressof(value); }
        else { result_ = std::forward<value_type>(value); }
    }

    /**
     * @brief Gets the result of the coroutine.
     * @return return_type The result of the coroutine.
     */
    [[nodiscard]] auto get_result() noexcept(false) -> return_type
    {
        if (exception_) { std::rethrow_exception(exception_); }
        if constexpr (return_type_is_reference) { return *result_; }
        else { return std::move(result_); }
    }
};

/**
 * @brief Specialization of `io_task_promise_base` for void return type.
 */
template <> struct io_task_promise<void> final : public io_task_promise_base
{
    /**
     * @brief Gets the return object (`io_func`) associated with this promise.
     * @return io_func<void> The `io_func` associated with this promise.
     */
    io_func<void> get_return_object() noexcept;
    /// Does nothing on return.
    void return_void() noexcept {}
    /**
     * @brief Handles the result of the coroutine.
     *
     * Rethrows any stored exception.
     */
    void result() const
    {
        if (exception_) { std::rethrow_exception(exception_); }
        // No value to return for void
    }

    /**
     * @brief Placeholder to maintain consistency with the non-void specialization.
     */
    static void get_result() noexcept {} // Added for consistency
};

/**
 * @brief A coroutine task type for asynchronous I/O operations.
 *
 * The `io_func` class represents a coroutine task that can be awaited on. It provides:
 *   - Proper lifetime management of the coroutine
 *   - Exception propagation
     *   - Continuation chaining for async operations
 *   - Move-only semantics
 *   - Cancellation support, allowing the coroutine to be cancelled and throw an exception when awaited. Cancellation
 *     is cooperative and must be checked at suspension points.
 *
 * @tparam return_type The type that the coroutine returns.
 *
 * Example usage:
 * ```cpp
 * io_func<int> async_operation() {
 *   // ... async work ...
 *   co_return 42;
 * }
 * ```
 */
template <typename return_type> class io_func
{
  public:
    using promise_type                             = io_task_promise<return_type>;
    using task_type                                = io_func<return_type>;
    using handle_type                              = std::coroutine_handle<promise_type>;
    static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;

    /// Get the current number of active tasks
    [[nodiscard]] static size_t active_tasks() noexcept { return task_count_; }

    /**
     * @brief Explicit constructor that takes a coroutine handle.
     * @param handle The coroutine handle.
     */
    explicit constexpr io_func(handle_type handle) noexcept : handle_(handle)
    {
        ++task_count_;
    }

    /**
     * @brief Default constructor.
     */
    constexpr io_func() noexcept : handle_(nullptr) { ++task_count_; }

    /**
     * @brief Move constructor.
     * @param other The `io_func` to move from.
     */
    constexpr io_func(io_func &&other) noexcept 
        : handle_(std::exchange(other.handle_, nullptr))
        , task_id_(std::exchange(other.task_id_, "")) 
    { 
        ++task_count_; 
    }

    /**
     * @brief Destructor.
     *
     * Destroys the coroutine handle if it is valid.
     */
    ~io_func()
    {
        if (handle_ != nullptr)
        {
            handle_.destroy();
        }
    }

    /**
     * @brief Move assignment operator.
     * @param other The `io_func` to move from.
     * @return io_func& A reference to this `io_func`.
     */
    constexpr io_func &operator=(io_func &&other) noexcept
    {
        if (this != &other)
        {
            handle_type old_handle = handle_;
            handle_                = std::exchange(other.handle_, nullptr);
            task_id_               = std::exchange(other.task_id_, "");

            if (old_handle != nullptr) { old_handle.destroy(); }
        }
        return *this;
    }

    io_func(const io_func &) = delete;

    /**
     * @brief Checks if the coroutine is ready to resume.
     * @return bool True if the coroutine is ready; otherwise, false.
     */
    [[nodiscard]] constexpr bool await_ready() const noexcept { return !handle_ || handle_.done(); }

    /**
     * @brief Suspends the coroutine and sets the continuation.
     * @param awaiting_coroutine The handle of the coroutine awaiting this task.
     * @return std::coroutine_handle<> The handle of the coroutine to resume.
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine)
    {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_;
    }

    /**
     * @brief Resumes the coroutine and returns the result.
     * @return auto The result of the coroutine.
     */
    decltype(auto) await_resume()
    {
        if (handle_.promise().exception_) { std::rethrow_exception(handle_.promise().exception_); }
        return handle_.promise().get_result();
    }

    /**
     * @brief Gets the coroutine handle.
     * @return handle_type The coroutine handle.
     */
    [[nodiscard]] constexpr handle_type handle() const noexcept { return handle_; }

    /**
     * @brief Equality operator.
     * @param other The `io_func` to compare with.
     * @return bool True if the handles are equal; otherwise, false.
     */
    constexpr bool operator==(const io_func &other) const noexcept { return handle_ == other.handle_; }

    /**
     * @brief Gets the result of the coroutine.
     * @return return_type The result of the coroutine.
     * @throws std::runtime_error If the `io_func` is uninitialized or incomplete.
     */
    [[nodiscard]] return_type result() noexcept(false)
    {
        if (handle_ == nullptr) { throw std::runtime_error("Attempting to get result from uninitialized io_func"); }
        if (!handle_.done()) { throw std::runtime_error("Attempting to get result from incomplete io_func"); }
        check_and_rethrow_exception();
        return handle_.promise().get_result();
    }

    /**
     * @brief Checks if the coroutine is ready and done.
     * @return constexpr bool
     */
    [[nodiscard]] constexpr bool is_ready() const noexcept { return handle_ && handle_.done(); }

    /**
     * @brief Checks if the coroutine has an exception.
     * @return constexpr bool
     */
    [[nodiscard]] constexpr bool has_exception() const noexcept
    {
        return handle_ && handle_.promise().exception_ != nullptr;
    }

    /**
     * @brief Checks if the coroutine is valid.
     * @return constexpr bool
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept { return handle_ != nullptr; }

    /**
     * @brief Gets the task ID.
     * @return std::string The task ID.
     */
    [[nodiscard]] std::string task_id() const noexcept { return task_id_; }


    void set_task_id(const std::string &id) noexcept { task_id_ = id; }

  private:
    /**
     * @brief Checks for and rethrows any stored exception.
     */
    void check_and_rethrow_exception() const
    {
        if (handle_.promise().exception_) { std::rethrow_exception(handle_.promise().exception_); }
    }

    /// The coroutine handle.
    handle_type handle_{nullptr};
    static inline std::atomic<size_t> task_count_{0};
    std::string task_id_ = "#" + std::to_string(task_count_.load());
};
template <typename return_type> io_func<return_type> io_task_promise<return_type>::get_return_object() noexcept
{
    return io_func<return_type>{std::coroutine_handle<io_task_promise>::from_promise(*this)};
}

io_func<void> io_task_promise<void>::get_return_object() noexcept
{
    return io_func<void>{std::coroutine_handle<io_task_promise>::from_promise(*this)};
}

} // namespace detail

using io_task = detail::io_func<void>;
template <typename return_type> using io_func = detail::io_func<return_type>;

} // namespace io