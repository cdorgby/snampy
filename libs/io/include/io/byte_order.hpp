#pragma once
#include <cstdint>
#include <type_traits>

namespace io {
namespace byte_order {

// Compile-time endianness detection
enum class endian {
    little = 0,
    big = 1,
    native = 2
};

#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        constexpr endian native_endian = endian::little;
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        constexpr endian native_endian = endian::big;
    #else
        #error "Unsupported byte order"
    #endif
#elif defined(_WIN32)
    constexpr endian native_endian = endian::little;
#else
    // Compile-time detection using char array
    constexpr endian native_endian = [] {
        const union { uint16_t u16; uint8_t u8[2]; } detector = { 0x0102 };
        return (detector.u8[0] == 0x01) ? endian::big : endian::little;
    }();
#endif

// Optimized byte swap functions using compiler builtins
constexpr inline uint16_t byte_swap(uint16_t value) {
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_bswap16(value);
    #else
        return (value << 8) | (value >> 8);
    #endif
}

inline uint32_t byte_swap(uint32_t value) {
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_bswap32(value);
    #else
        return ((value & 0x000000FFU) << 24) | 
               ((value & 0x0000FF00U) << 8) | 
               ((value & 0x00FF0000U) >> 8) | 
               ((value & 0xFF000000U) >> 24);
    #endif
}

inline uint64_t byte_swap(uint64_t value) {
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_bswap64(value);
    #else
        return ((value & 0x00000000000000FFULL) << 56) | 
               ((value & 0x000000000000FF00ULL) << 40) | 
               ((value & 0x0000000000FF0000ULL) << 24) | 
               ((value & 0x00000000FF000000ULL) << 8) | 
               ((value & 0x000000FF00000000ULL) >> 8) | 
               ((value & 0x0000FF0000000000ULL) >> 24) | 
               ((value & 0x00FF000000000000ULL) >> 40) | 
               ((value & 0xFF00000000000000ULL) >> 56);
    #endif
}

// Add support for floating point types with safe bit-level manipulation
template<typename T>
typename std::enable_if_t<std::is_floating_point_v<T>, T>
host_to_network(T value) {
    union {
        T f;
        typename std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t> i;
    } u;
    
    u.f = value;
    
    if constexpr (native_endian == endian::little) {
        if constexpr (sizeof(T) == 4)
            u.i = byte_swap(u.i);
        else if constexpr (sizeof(T) == 8)
            u.i = byte_swap(u.i);
    }
    
    return u.f;
}

// Template constraint for integral types to avoid ambiguity with the floating point version
template<typename T>
typename std::enable_if_t<std::is_integral_v<T>, T>
host_to_network(T value) {
    if constexpr (native_endian == endian::little) {
        if constexpr (sizeof(T) == 1) return value;
        if constexpr (sizeof(T) == 2) return byte_swap(static_cast<uint16_t>(value));
        if constexpr (sizeof(T) == 4) return byte_swap(static_cast<uint32_t>(value));
        if constexpr (sizeof(T) == 8) return byte_swap(static_cast<uint64_t>(value));
    }
    return value; // Already in network order (big endian)
}

template<typename T>
T network_to_host(T value) {
    return host_to_network(value); // Same operation, just named for clarity
}

} // namespace byte_order
} // namespace io
