#include <common/log.hpp>
#include <common/catch.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>

// Mock log sink for testing
class mock_log_sink : public log_sink {
public:
    struct captured_log {
        log_level level;
        std::string message;
        std::string file;
        uint32_t line;
    };

    void log(const log_line_dispatcher&, const log_line& line) const override {
        captured_logs.push_back({
            line.level_,
            std::string(line.message_, line.message_size_),
            std::string(line.file_),
            line.line_
        });
    }

    void clear() {
        captured_logs.clear();
    }

    mutable std::vector<captured_log> captured_logs;
};

// Define formatter before using CustomType
struct CustomType {};

template<>
struct std::formatter<CustomType> : std::formatter<string_view> {
    auto format(const CustomType&, format_context& ctx) const {
        return std::formatter<string_view>::format("CustomType", ctx);
    }
};

class LoggingTest {
public:  // Changed from protected to public
    static mock_log_sink mock_sink;
    static log_sink* original_sink;

    LoggingTest() {
        auto& dispatcher = log_line_dispatcher::instance();
        original_sink = dispatcher.get_sink(0);
        dispatcher.set_sink(0, &mock_sink);
        mock_sink.clear();
    }

    ~LoggingTest() {
        auto& dispatcher = log_line_dispatcher::instance();
        dispatcher.set_sink(0, original_sink);
    }
};

mock_log_sink LoggingTest::mock_sink;
log_sink* LoggingTest::original_sink = nullptr;

TEST_CASE_METHOD(LoggingTest, "Logging modern C++ functionality", "[log]") {
    SECTION("Verify captured log messages") {
        LOG(info) << "Test message";
        
        REQUIRE(mock_sink.captured_logs.size() == 1);
        auto& log = mock_sink.captured_logs[0];
        CHECK(log.level == log_level::info);
        CHECK(log.message == "Test message");
    }
}

TEST_CASE("Logging modern C++ functionality", "[log]") {
    SECTION("std::format integration") {
        const auto value = 42;
        const auto text = "test";
        LOG(info).format("Values: {}, {}", value, text);
    }

    SECTION("Concepts and constraints") {
        CustomType ct;
        LOG(debug) << ct;
    }

    SECTION("Smart pointer handling") {
        auto ptr = std::make_shared<int>(42);
        LOG(info) << ptr;
        
        std::weak_ptr<int> weak = ptr;
        LOG(info) << weak;
        
        ptr.reset();
        LOG(info) << weak; // Should show as expired
    }

    SECTION("Simple log message") {
        LOG(info) << "This is a test message.";
        // Add assertions here to check if the message was logged correctly.
        // This might involve capturing stdout or using a custom log sink.
    }

    SECTION("Log message with formatting") {
        int value = 42;
        LOG(debug).printf("The value is: %d", value);
        // Add assertions to check the formatted message.
    }

    SECTION("Log message with std::format") {
        std::string name = "Snampy";
        LOG(warn).format("Hello, {}!", name);
    }

    SECTION("Multiple log levels") {
        LOG(trace) << "Trace message";
        LOG(debug) << "Debug message";
        LOG(info) << "Info message";
        LOG(warn) << "Warning message";
        LOG(error) << "Error message";
        LOG(fatal) << "Fatal message";
    }
}

