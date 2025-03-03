#pragma once

#include <io/io.hpp>
#include <queue>
#include <optional>
#include <limits>
#include <concepts>
#include <span>
#include <ranges>
#include <type_traits>

namespace io
{
/**
 * Implementation of different types of mailboxes for inter-task communication.
 *
 * The mailbox is a simple queue that can be used to send messages between tasks.
 * In this single-threaded implementation, all operations happen within the same io_loop
 * thread, so no mutex synchronization is needed.
 *
 * Currently there are three types of mailboxes:
 * - io_mbox - a simple mailbox that can have a single reader and multiple writers
 * - io_mbox_any - a mailbox that can have multiple readers and writers, but only one reader will receive the message
 * - io_mbox_all - a mailbox that can have multiple readers and writers, and all readers will receive the message
 * 
 * Note: The implementation is designed for single-threaded environments where the io_loop
 * runs in a single thread. There are other implementations for communication between tasks running in different threads.
 * 
 */

/**
 * A simple mailbox that can have a single reader and multiple writers.
 *
 * @tparam T The type of messages to be sent through the mailbox
 *
 * @note This mailbox is not thread-safe and should only be used in single-threaded environments.
 *
 * ## Theory of Operation
 *
 * The mailbox operates using a producer-consumer pattern with these characteristics:
 *
 * 1. **Message Queue**: Messages are stored in a FIFO queue when no readers are waiting.
 *    When a reader becomes available, the oldest message is delivered first.
 *
 * 2. **Reader Registration**: When a task calls `read()`, it becomes a "reader":
 *    - If messages are already in the queue, the reader immediately receives the oldest message.
 *    - If the queue is empty, the reader is registered in a waiting list and its coroutine is suspended.
 *    - If multiple readers are waiting, they form a queue and receive messages in registration order.
 *
 * 3. **Message Delivery**: When a message is sent:
 *    - If readers are waiting, the message is delivered directly to the longest-waiting reader,
 *      which resumes its coroutine with the message as its result.
 *    - If no readers are waiting, the message is queued for future readers.
 *
 * 4. **Timeout Handling**: Reads can specify a timeout. If no message arrives before
 *    the timeout expires, the read operation returns `std::nullopt`.
 *
 * 5. **Bounded Queue**: The mailbox can optionally have a maximum size. When the queue
 *    reaches this limit, the oldest messages are dropped to make room for new ones (FIFO behavior).
 *
 * 6. **Mailbox Closing**: When a mailbox is closed:
 *    - All waiting readers are notified and receive `std::nullopt`.
 *    - All queued messages are discarded.
 *    - New messages are rejected.
 *    - New read operations immediately return `std::nullopt`.
 *
 * 7. **Resource Management**: If a mailbox is destroyed while readers are still
 *    registered, they are properly notified to prevent accessing the destroyed mailbox.
 */
template <typename T>
class io_mbox {
private:
    // Forward declaration for the mailbox reader
    class mailbox_reader;
    
    // The message queue
    std::queue<T> message_queue_;
    
    // List of waiting readers
    std::vector<mailbox_reader*> readers_;
    
    // Reference to the io_loop
    io_loop& loop_;
    
    // Maximum queue size (0 = unlimited)
    size_t max_queue_size_;
    
    // Flag to indicate if the mailbox is closed
    bool is_closed_ = false;
    
    // The mailbox reader class that implements the awaitable pattern
    class mailbox_reader : public detail::io_promise {
    public:
        mailbox_reader(io_mbox& mbox, io_loop& loop, time_point_t deadline)
            : io_promise(loop, deadline), mbox_(&mbox) {}
        
        ~mailbox_reader() {
            // Make sure we're unregistered from the mailbox
            unregister();
        }
        
        bool await_ready() noexcept override {
            // If the mailbox is closed or null, we're ready immediately with no message
            if (!mbox_ || mbox_->is_closed_) {
                return true;
            }
            
            // Check for timeout before we even start
            if (timeout()) {
                is_timed_out_ = true;
                return true;
            }
            
            // If there are messages available, take one and return ready
            if (!mbox_->message_queue_.empty()) {
                result_ = std::move(mbox_->message_queue_.front());
                mbox_->message_queue_.pop();
                return true;
            }
            
            // No messages available, register for notification
            mbox_->readers_.push_back(this);
            is_registered_ = true;
            
            // Register with the IO loop
            waiter_.add();
            return false;
        }
        
        void await_suspend(std::coroutine_handle<> h) noexcept override {
            io_promise::await_suspend(h);
        }
        
        std::optional<T> await_resume() noexcept {
            // Always remove from IO loop
            waiter_.remove();
            
            // Unregister from the mailbox
            unregister();
            
            // Handle timeout
            if (waiter_.result() == io_result::timeout || is_timed_out_) {
                return std::nullopt;
            }
            
            // Handle closed or null mailbox
            if (!mbox_ || mbox_->is_closed_) {
                return std::nullopt;
            }
            
            // Return result if we have one
            if (result_.has_value()) {
                return std::move(result_);
            }
            
            // Check for cancellation
            if (waiter_.result() == io_result::cancelled) {
                return std::nullopt;
            }
            
            // Check if a message arrived just before resuming
            if (mbox_ && !mbox_->message_queue_.empty()) {
                auto msg = std::move(mbox_->message_queue_.front());
                mbox_->message_queue_.pop();
                return std::move(msg);
            }
            
            // Default case - no message
            return std::nullopt;
        }
        
        // Called when a message is available for this reader
        void deliver_message(T&& message) {
            // Store the message
            result_.emplace(std::move(message));
            
            // Mark as no longer registered
            is_registered_ = false;
            
            // Complete the waiter to resume the coroutine
            waiter_.complete(io_result::done);
        }
        
        // Called when the mailbox is being destroyed
        void detach_mailbox() {
            mbox_ = nullptr;
            
            // If we're waiting, complete with cancelled
            if (is_registered_) {
                is_registered_ = false;
                waiter_.complete(io_result::cancelled);
            }
        }
        
    private:
        // Unregister from the mailbox if still registered
        void unregister() {
            if (is_registered_ && mbox_) {
                // Use C++20 std::ranges to find and erase in one step
                auto it = std::ranges::find(mbox_->readers_, this);
                if (it != mbox_->readers_.end()) {
                    mbox_->readers_.erase(it);
                }
                is_registered_ = false;
            }
        }
        
        io_mbox* mbox_;  // Pointer instead of reference to handle mailbox destruction
        std::optional<T> result_;
        bool is_registered_ = false;
        bool is_timed_out_ = false;
    };
    
public:
    /**
     * Create a new mailbox.
     * 
     * @param loop The io_loop this mailbox will use
     * @param max_messages Maximum number of messages (0 = unlimited)
     */
    explicit io_mbox(io_loop& loop, size_t max_messages = 0) noexcept
        : loop_(loop), max_queue_size_(max_messages) {}
    
    /**
     * Destructor - detach all readers
     */
    ~io_mbox() {
        // Detach all readers to prevent them from accessing this mailbox
        for (auto* reader : readers_) {
            reader->detach_mailbox();
        }
        readers_.clear();
    }
    
    /**
     * Send a message to the mailbox.
     * This overload handles both lvalue and rvalue references using
     * perfect forwarding to avoid ambiguity.
     * 
     * @param message The message to send
     * @return true if the message was accepted, false if the mailbox is closed
     */
    template <typename U>
    requires std::convertible_to<U, T>
    bool send(U&& message) noexcept {
        // Don't accept messages if closed
        if (is_closed_) {
            return false;
        }
        
        // If there are waiting readers, deliver to the first one
        if (!readers_.empty()) {
            // Get the first reader
            mailbox_reader* reader = readers_.front();
            readers_.erase(readers_.begin());
            
            // Deliver the message using perfect forwarding
            reader->deliver_message(T(std::forward<U>(message)));
            return true;
        }
        
        // No waiting readers, queue the message
        
        // Check if we need to make room
        if (max_queue_size_ > 0 && message_queue_.size() >= max_queue_size_) {
            message_queue_.pop(); // Remove oldest message
        }
        
        // Add the new message using perfect forwarding
        message_queue_.push(T(std::forward<U>(message)));
        return true;
    }

    /**
     * Send a message to the mailbox.
     * This overload handles lvalue references.
     * 
     * @param message The message to send
     * @return true if the message was accepted, false if the mailbox is closed
     */
    bool send(const T& message) noexcept {
        // Don't accept messages if closed
        if (is_closed_) {
            return false;
        }
        
        // If there are waiting readers, deliver to the first one
        if (!readers_.empty()) {
            // Get the first reader
            mailbox_reader* reader = readers_.front();
            readers_.erase(readers_.begin());
            
            // Deliver the message (by making a copy)
            reader->deliver_message(T(message));
            return true;
        }
        
        // No waiting readers, queue the message
        
        // Check if we need to make room
        if (max_queue_size_ > 0 && message_queue_.size() >= max_queue_size_) {
            message_queue_.pop(); // Remove oldest message
        }
        
        // Add the new message
        message_queue_.push(message);
        return true;
    }

    /**
     * Send a message to the mailbox (rvalue reference overload for better performance).
     * 
     * @param message The message to send
     * @return true if the message was accepted, false if the mailbox is closed
     */
    bool send(T&& message) noexcept {
        // Don't accept messages if closed
        if (is_closed_) {
            return false;
        }
        
        // If there are waiting readers, deliver to the first one
        if (!readers_.empty()) {
            // Get the first reader
            mailbox_reader* reader = readers_.front();
            readers_.erase(readers_.begin());
            
            // Deliver the message
            reader->deliver_message(std::move(message));
            return true;
        }
        
        // No waiting readers, queue the message
        
        // Check if we need to make room
        if (max_queue_size_ > 0 && message_queue_.size() >= max_queue_size_) {
            message_queue_.pop(); // Remove oldest message
        }
        
        // Add the new message
        message_queue_.push(std::move(message));
        return true;
    }
    
    /**
     * Send a message to the mailbox, constructing it in-place from the arguments.
     * This is useful for complex types that can be constructed from multiple arguments.
     * 
     * @param args Arguments to forward to the constructor of T
     * @return true if the message was accepted, false if the mailbox is closed
     */
    template <typename... Args>
    requires std::constructible_from<T, Args...>
    bool emplace(Args&&... args) noexcept {
        // Don't accept messages if closed
        if (is_closed_) {
            return false;
        }
        
        if (!readers_.empty()) {
            // Get the first reader
            mailbox_reader* reader = readers_.front();
            readers_.erase(readers_.begin());
            
            // Deliver a new message constructed from the arguments
            reader->deliver_message(T(std::forward<Args>(args)...));
            return true;
        }
        
        // No waiting readers, queue the message
        
        // Check if we need to make room
        if (max_queue_size_ > 0 && message_queue_.size() >= max_queue_size_) {
            message_queue_.pop(); // Remove oldest message
        }
        
        // Use emplace to avoid extra moves
        if constexpr (requires { message_queue_.emplace(std::forward<Args>(args)...); }) {
            message_queue_.emplace(std::forward<Args>(args)...);
        } else {
            // Fall back to constructing and pushing if emplace is not available
            message_queue_.push(T(std::forward<Args>(args)...));
        }
        return true;
    }
    
    /**
     * Read a message from the mailbox.
     * 
     * @param timeout Optional timeout
     * @return A co_awaitable that resolves to an optional containing the message
     */
    [[nodiscard]] auto read(time_point_t timeout = time_point_t::max()) noexcept {
        return mailbox_reader(*this, loop_, timeout);
    }
    
    /**
     * Check if the mailbox has any messages.
     * 
     * @return true if there are messages available
     */
    [[nodiscard]] bool has_messages() const noexcept {
        return !message_queue_.empty();
    }
    
    /**
     * Clear all messages from the mailbox.
     */
    void clear() noexcept {
        std::queue<T> empty;
        std::swap(message_queue_, empty);
    }
    
    /**
     * Close the mailbox.
     * This prevents new messages from being sent and causes all
     * waiting readers to receive nullopt.
     */
    void close() noexcept {
        if (is_closed_) {
            return; // Already closed
        }
        
        is_closed_ = true;
        
        // Wake up all waiting readers
        auto readers_copy = readers_; // Make a copy since we'll be modifying the original
        readers_.clear();
        
        for (auto* reader : readers_copy) {
            reader->waiter_.complete(io_result::cancelled);
        }
        
        // Clear all messages
        clear();
    }

    /**
     * Get the current number of messages.
     * 
     * @return The number of queued messages
     */
    [[nodiscard]] size_t size() const noexcept {
        return message_queue_.size();
    }
    
    /**
     * Get the maximum number of messages.
     * 
     * @return The maximum queue size (0 = unlimited)
     */
    [[nodiscard]] size_t max_size() const noexcept {
        return max_queue_size_;
    }

    /**
     * Check if the mailbox has been closed.
     * 
     * @return true if the mailbox is closed
     */
    [[nodiscard]] bool is_closed() const noexcept {
        return is_closed_;
    }
};

/**
 * A mailbox that can have multiple readers and writers, but only one reader will receive each message.
 * Uses C++20 concepts to ensure message types are moveable.
 */
template <std::movable T>
class io_mbox_any {
};

/**
 * A mailbox that can have multiple readers and writers, and all readers will receive each message.
 * This implements a publish-subscribe pattern where each message is delivered to all subscribers.
 * 
 * @tparam T The type of messages to be sent through the mailbox
 */
template <std::movable T>
class io_mbox_all {
};

} // namespace io
