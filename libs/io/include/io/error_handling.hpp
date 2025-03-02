#pragma once

#include <system_error>
#include <string>
#include <io/common.hpp>

namespace io {

/**
 * @brief Custom error category for I/O-specific errors.
 */
class io_error_category : public std::error_category {
public:
    static const io_error_category& instance() {
        static io_error_category category;
        return category;
    }

    const char* name() const noexcept override {
        return "io_error";
    }

    std::string message(int value) const override {
        switch (value) {
            case 1: return "Operation aborted";
            case 2: return "I/O descriptor closed";
            case 3: return "Operation timeout";
            // Add more specific I/O error codes as needed
            default: return "Unknown I/O error";
        }
    }
};

/**
 * @brief Error codes specific to I/O operations.
 */
enum class io_errc {
    operation_aborted = 1,
    descriptor_closed = 2,
    operation_timeout = 3
};

/**
 * @brief Create an error code from an I/O error.
 * @param e The I/O error code.
 * @return A std::error_code representing the error.
 */
inline std::error_code make_error_code(io_errc e) {
    return {static_cast<int>(e), io_error_category::instance()};
}

/**
 * @brief Create an error code from system errno.
 * @return A std::error_code representing the current system error.
 */
inline std::error_code system_error() {
    return std::error_code(errno, std::system_category());
}

/**
 * @brief Convert io_result to appropriate error code.
 * @param result The I/O result to convert.
 * @return A std::error_code representing the result.
 */
inline std::error_code result_to_error(io_result result) {
    switch (result) {
        case io_result::timeout: 
            return make_error_code(io_errc::operation_timeout);
        case io_result::closed: 
            return make_error_code(io_errc::descriptor_closed);
        case io_result::cancelled:
            return make_error_code(io_errc::operation_aborted);
        case io_result::error:
            return system_error(); // Default to current system error
        default:
            return {};
    }
}

} // namespace io

// Enable std::error_code to work with io_errc
namespace std {
    template <> 
    struct is_error_code_enum<io::io_errc> : true_type {};
}
