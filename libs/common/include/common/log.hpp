#pragma once

#include <cstdint>
#include <cstring>
#include <concepts>
#include <string>
#include <string_view>
#include <format>
#include <chrono>
#include <iostream>
#include <source_location>
#include <syslog.h>
#include <array>
#include <span>
#include <memory>

// formatter specializations for smart pointers
template<typename T>
struct std::formatter<std::shared_ptr<T>, char> : std::formatter<const void*, char> {
    auto format(const std::shared_ptr<T>& ptr, format_context& ctx) const {
        if (!ptr) return format_to(ctx.out(), "nullptr");
        return std::formatter<const void*, char>::format(ptr.get(), ctx);
    }
};

// Specialization for weak pointers
template<typename T>
struct std::formatter<std::weak_ptr<T>, char> : std::formatter<const void*, char> {
    auto format(const std::weak_ptr<T>& ptr, format_context& ctx) const {
        if (auto shared = ptr.lock()) {
            return std::formatter<const void*, char>::format(shared.get(), ctx);
        }
        return format_to(ctx.out(), "(expired)");
    }
};

/**
 * @brief Enumeration of available log levels in ascending order of severity
 */
enum class log_level : int8_t
{
    nolog = -1, ///< No logging
    trace = 0,  ///< Finest-grained information
    debug = 1,  ///< Debugging information
    info  = 2,  ///< General information
    warn  = 3,  ///< Warning messages
    error = 4,  ///< Error messages
    fatal = 5,  ///< Critical errors
};

constexpr log_level MIN_LOG_LEVEL = log_level::trace;

const std::array<const char *, 6> log_level_colors = {
    "\033[37m", // trace
    "\033[36m", // debug
    "\033[32m", // info
    "\033[33m", // warn
    "\033[31m", // error
    "\033[35m", // fatal
};

const std::array<const char *, 6> log_level_names = {
    "TRACE",
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL",
};

/**
 * @brief Concept for types that can be logged
 * @tparam T The type to check
 */
template<typename T>
concept Loggable = requires(T value, std::string& str) {
    { std::format("{}", value) } -> std::convertible_to<std::string>;
};

template<typename T>
concept SmartPointer = requires(T ptr) {
    typename T::element_type;
    { ptr.get() } -> std::convertible_to<typename T::element_type*>;
};

/**
 * @brief Interface for log output destinations
 */
struct log_sink {
    virtual ~log_sink() = default;
    virtual void log(const struct log_line_dispatcher& dispatcher, const struct log_line& line) const = 0;
};

/**
 * @brief Standard output implementation of log sink
 */
class log_sink_stdout final: public log_sink
{
    public:
      void log(const struct log_line_dispatcher &dispatcher, const struct log_line &line) const override;
};

/**
 * @brief Manages log message dispatching to multiple sinks
 */
struct log_line_dispatcher
{
    static constexpr size_t MAX_SINKS = 4;

    void dispatch(const struct log_line &line) const
    {
        for (size_t i = 0; i < sink_count_; ++i) { sinks_[i]->log(*this, line); }
    }

    static log_line_dispatcher &instance()
    {
        static log_line_dispatcher instance;
        static log_sink_stdout log_sink_stdout;
        static bool initialized = false;

        if (!initialized)
        {
            instance.add_sink(&log_sink_stdout);
            initialized = true;
            openlog("log", LOG_PID | LOG_NDELAY, LOG_USER);
        }
        return instance;
    }

    constexpr auto start_time() const noexcept { return start_time_; }
    constexpr auto start_time_us() const noexcept { return start_time_us_; }

    void add_sink(log_sink *sink)
    {
        if (sink_count_ < MAX_SINKS) { sinks_[sink_count_++] = sink; }
    }

    log_sink *get_sink(size_t index) const { return index < sink_count_ ? sinks_[index] : nullptr; }

    void set_sink(size_t index, log_sink *sink)
    {
        if (index < MAX_SINKS)
        {
            sinks_[index] = sink;
            if (index >= sink_count_) { sink_count_ = index + 1; }
        }
    }

  private:
    log_line_dispatcher()
    : start_time_(std::chrono::steady_clock::now()),
      start_time_us_(std::chrono::duration_cast<std::chrono::microseconds>(start_time_.time_since_epoch()).count()),
      sink_count_(0)
    {
    }

    std::chrono::steady_clock::time_point start_time_;
    int64_t start_time_us_;
    log_sink *sinks_[MAX_SINKS]; // Fixed array instead of vector
    size_t sink_count_{0};
};

/**
 * @brief Represents a single log message with metadata
 * 
 * This class handles the formatting and buffering of a single log message
 * along with its associated metadata (timestamp, level, location).
 */
struct log_line
{
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;
    static constexpr int PADDING_FILE_LINE   = 45;
    using timestamp_type                     = std::chrono::steady_clock::time_point;
    using buffer_view                        = std::span<char>;

    char message_[MAX_MESSAGE_SIZE];
    size_t message_size_{0};
    timestamp_type timestamp_{};
    int64_t timestamp_us_{0}; // Cache microsecond timestamp
    log_level level_{0};
    std::string_view file_{};
    uint32_t line_{0};

    log_line() = delete;

    // must have timestamp, level, file, line
    log_line(log_level level, std::string_view file, uint64_t line, timestamp_type timestamp = std::chrono::steady_clock::now())
    : timestamp_(timestamp),
      timestamp_us_(std::chrono::duration_cast<std::chrono::microseconds>(timestamp_.time_since_epoch()).count()),
      level_(level),
      file_(file),
      line_(line)
    {
    }

    log_line(log_level level, const std::source_location &location = std::source_location::current())
    : timestamp_(std::chrono::steady_clock::now()),
      timestamp_us_(std::chrono::duration_cast<std::chrono::microseconds>(timestamp_.time_since_epoch()).count()),
      level_(level),
      file_(location.file_name()),
      line_(location.line())
    {
    }

    // no copy no move no assign
    log_line(const log_line &)            = delete;
    log_line &operator=(const log_line &) = delete;
    log_line(log_line &&)                 = delete;
    log_line &operator=(log_line &&)      = delete;

    ~log_line()
    {
        if (level_ == log_level::nolog) { return; }
        // dispatch to log system
        if (message_size_ > 0) { log_line_dispatcher::instance().dispatch(*this); }
    }

    void print(std::string_view str)
    {
        if (level_ == log_level::nolog) { return; }
        size_t copy_size = std::min(str.size(), MAX_MESSAGE_SIZE - message_size_);
        memcpy(message_ + message_size_, str.data(), copy_size);
        message_size_ += copy_size;
    }

    template <typename... Args> void printf(const char *format, Args &&...args)
    {
        if (level_ == log_level::nolog) { return; }
        char buffer[MAX_MESSAGE_SIZE];
        int len = snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
        if (len > 0) { print(std::string_view(buffer, len)); }
    }

    // Helper method that returns *this for chaining
    template <typename... Args> log_line &printfmt(const char *format, Args &&...args)
    {
        if (level_ == log_level::nolog) { return *this; }
        printf(format, std::forward<Args>(args)...);
        return *this;
    }

    template <typename... Args> log_line &format(std::format_string<Args...> fmt, Args &&...args)
    {
        if (level_ == log_level::nolog) { return *this; }
        print(std::format(fmt, std::forward<Args>(args)...));
        return *this;
    }

    // Helper method that returns *this for chaining
    template <typename... Args> log_line &fmtprint(std::format_string<Args...> fmt, Args &&...args)
    {
        if (level_ == log_level::nolog) { return *this; }
        format(fmt, std::forward<Args>(args)...);
        return *this;
    }

    // Generic version for any formattable type
    template <typename T>
        requires Loggable<T>
    log_line &operator<<(const T &value)
    {
        if (level_ == log_level::nolog) { return *this; }
        print(std::format("{}", value));
        return *this;
    }

    // Keep only the raw pointer handling
    template <typename T> log_line &operator<<(T *ptr)
    {
        if (level_ == log_level::nolog) { return *this; }
        if (ptr == nullptr) { print("nullptr"); }
        else { print(std::format("{}", static_cast<const void *>(ptr))); }
        return *this;
    }

    // Keep specialized versions for common types
    log_line &operator<<(std::string_view str)
    {
        if (level_ == log_level::nolog) { return *this; }
        print(str);
        return *this;
    }

    log_line &operator<<(const char *str)
    {
        if (level_ == log_level::nolog) { return *this; }
        print(std::string_view(str));
        return *this;
    }

    log_line &operator<<(const std::string &str)
    {
        if (level_ == log_level::nolog) { return *this; }
        print(std::string_view(str));
        return *this;
    }

    log_line &operator<<(int value)
    {
        if (level_ == log_level::nolog) { return *this; }
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d", value);
        print(std::string_view(buf, len));
        return *this;
    }

    log_line &operator<<(unsigned int value)
    {
        if (level_ == log_level::nolog) { return *this; }
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%u", value);
        print(std::string_view(buf, len));
        return *this;
    }

    log_line &operator<<(void *ptr)
    {
        if (level_ == log_level::nolog) { return *this; }
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%p", ptr);
        print(std::string_view(buf, len));
        return *this;
    }

    log_line &operator<<(log_line &(*func)(log_line &)) { return func(*this); }
};

// our own endl for log_line
log_line& endl(log_line &line)
{
    line.message_[line.message_size_++] = '\n';
    log_line_dispatcher::instance().dispatch(line);
    return line;
}

// Non-constexpr version since std::format isn't constexpr in C++20
inline std::string_view format_file_location(const log_line &line, std::span<char> buffer)
{

    char temp[256];
    int len = snprintf(temp, sizeof(temp), "%s:%u", line.file_.data(), line.line_);
    if (len < log_line::PADDING_FILE_LINE) {
        std::fill_n(buffer.begin(), log_line::PADDING_FILE_LINE - len, ' ');
        std::memcpy(buffer.data() + (log_line::PADDING_FILE_LINE - len), temp, len);
        return std::string_view(buffer.data(), log_line::PADDING_FILE_LINE);
    }
    std::memcpy(buffer.data(), temp, len);
    return std::string_view(buffer.data(), len);
}

void log_sink_stdout::log(const struct log_line_dispatcher &dispatcher, const log_line &line) const
{
    // Direct integer arithmetic instead of chrono operations
    int64_t diff_us = line.timestamp_us_ - dispatcher.start_time_us();
    int64_t ms = diff_us / 1000;
    int64_t us = std::abs(diff_us % 1000);

    char loc_buffer[256];
    auto loc = format_file_location(line, std::span<char>(loc_buffer, sizeof(loc_buffer)));

    // Use stack buffer for formatting
    char buffer[2048];
    int len = snprintf(buffer,
                       sizeof(buffer),
                       "%s%08lld.%03lld [%s] %.*s %.*s\033[0m\n",
                       log_level_colors[static_cast<int>(line.level_)],
                       static_cast<long long>(ms),
                       static_cast<long long>(us),
                       log_level_names[static_cast<int>(line.level_)],
                       static_cast<int>(loc.size()),
                       loc.data(),
                       static_cast<int>(line.message_size_),
                       line.message_);

    // Single write call
    fwrite(buffer, 1, len, stdout);
}

/**
 * @brief Global minimal log level. Messages below this level will be eliminated at compile time.
 */
inline constexpr log_level GLOBAL_MIN_LOG_LEVEL = log_level::debug;

/**
 * @brief Creates a log line with specified level and automatic source location.
 * Uses constexpr to (mostly) eliminate logging below GLOBAL_MIN_LOG_LEVEL at compile time.
 * @param _level The log level to use
 */
#define LOG(_level)                                                                                               \
    []()                                                                                                          \
    {                                                                                                             \
        constexpr log_level level = log_level::_level;                                                            \
        if constexpr (level >= GLOBAL_MIN_LOG_LEVEL) { return log_line(level, std::source_location::current()); } \
        else { return log_line(log_level::nolog, std::source_location::current()); }                              \
    }()
