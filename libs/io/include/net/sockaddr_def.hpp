#pragma once

#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <common/log.hpp>
#include <iostream>
#include <climits>

namespace io
{

/**
 * @brief Socket address wrapper class that handles IPv4, IPv6, and Unix domain sockets
 *
 * This class provides a unified interface for handling different types of socket addresses.
 * It supports parsing addresses from strings and provides methods for comparison and hashing.
 *
 * Format for address strings:
 * - IPv4: "address[:port]" or "address/prefix[:port]" (e.g., "192.168.1.1:8080", "10.0.0.0/24")
 * - IPv6: "[address][:port]" or "[address/prefix][:port]" (e.g., "[::1]:8080", "[2001:db8::]/64")
 * - Unix: "path" or "@abstract" (e.g., "/tmp/socket", "@abstract-socket")
 * - Wildcards: "*", "any", or specific forms like "0.0.0.0" and "::"
 *
 * Example usage:
 * @code
 * // Create an IPv4 TCP socket address
 * sock_addr addr1("192.168.1.1:80", AF_INET, SOCK_STREAM);
 * if (addr1.len() > 0) {
 *     std::cout << "Address: " << addr1.to_string() << std::endl;
 *     std::cout << "Port: " << addr1.port() << std::endl;
 * }
 *
 * // Create an IPv6 UDP socket address
 * sock_addr addr2("[::1]:53", AF_INET6, SOCK_DGRAM);
 *
 * // Create a Unix domain socket address
 * sock_addr addr3("/tmp/my.sock", AF_UNIX);
 *
 * // Create an abstract Unix domain socket (Linux-specific)
 * sock_addr addr4("@my-service", AF_UNIX);
 *
 * // Use as key in standard containers
 * std::unordered_map<sock_addr, std::string> addr_map;
 * addr_map[addr1] = "web server";
 * addr_map[addr2] = "dns server";
 * @endcode
 */
class sock_addr
{
public:
    /**
     * @brief Default constructor creates an empty socket address
     */
    sock_addr();

    /**
     * @brief Constructs from an IPv4 sockaddr_in structure
     * @param sa The sockaddr_in structure to copy from
     * @param prefix Network prefix length (0-32)
     * @param socktype Socket type (e.g., SOCK_STREAM, SOCK_DGRAM)
     * @param proto Protocol (e.g., IPPROTO_TCP, IPPROTO_UDP)
     */
    sock_addr(const struct sockaddr_in &sa, uint8_t prefix = 0, uint16_t socktype = SOCK_STREAM, uint16_t proto = 0);

    /**
     * @brief Constructs from an IPv6 sockaddr_in6 structure
     * @param sa The sockaddr_in6 structure to copy from
     * @param prefix Network prefix length (0-128)
     * @param socktype Socket type (e.g., SOCK_STREAM, SOCK_DGRAM)
     * @param proto Protocol (e.g., IPPROTO_TCP, IPPROTO_UDP)
     */
    sock_addr(const struct sockaddr_in6 &sa, uint8_t prefix = 0, uint16_t socktype = SOCK_STREAM, uint16_t proto = 0);

    /**
     * @brief Constructs from a Unix domain socket address structure
     * @param sa The sockaddr_un structure to copy from
     * @param socktype Socket type (e.g., SOCK_STREAM, SOCK_DGRAM)
     */
    sock_addr(const struct sockaddr_un &sa, uint16_t socktype = SOCK_STREAM);

    /**
     * @brief Constructs from a generic sockaddr structure
     * @param sa Pointer to the sockaddr structure
     * @param len Length of the address structure
     * @param prefix Network prefix length
     * @param socktype Socket type
     * @param proto Protocol
     */
    sock_addr(const struct sockaddr *sa, socklen_t len, uint8_t prefix = 0, uint16_t socktype = SOCK_STREAM, uint16_t proto = 0);

    /**
     * @brief Constructs from an address string
     * @param addrfull Address string in supported format
     * @param family Address family (AF_INET, AF_INET6, AF_UNIX, or AF_UNSPEC)
     * @param socktype Socket type
     * @param proto Protocol
     */
    sock_addr(const std::string &addrfull, uint16_t family = AF_UNSPEC, uint16_t socktype = SOCK_STREAM, uint16_t proto = 0);

    /**
     * @brief Constructs from separate address and port strings
     * @param addr Address string
     * @param port Port string
     * @param socktype Socket type
     * @param family Address family
     */
    sock_addr(const std::string_view &addr,
              const std::string_view &port,
              uint16_t family   = AF_UNSPEC,
              uint16_t socktype = SOCK_STREAM,
              uint16_t proto    = 0);

    /**
     * @brief Move constructor
     * @param other The sock_addr to move from
     */
    sock_addr(sock_addr&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other The sock_addr to move from
     * @return Reference to this object
     */
    sock_addr& operator=(sock_addr&& other) noexcept;

    /**
     * @brief Copy constructor
     * @param other The sock_addr to copy from
     */
    sock_addr(const sock_addr& other) noexcept;

    /**
     * @brief Copy assignment operator
     * @param other The sock_addr to copy from
     * @return Reference to this object
     */
    sock_addr& operator=(const sock_addr& other) noexcept;

    /**
     * @brief Gets the underlying sockaddr structure
     * @return Pointer to the sockaddr structure
     */
    struct sockaddr *sockaddr();

    /**
     * @brief Gets the underlying sockaddr structure (const version)
     * @return Const pointer to the sockaddr structure
     */
    const struct sockaddr *sockaddr() const;

    /**
     * @brief Gets the length of the address structure
     * @return Address structure length in bytes
     */
    socklen_t len() const;

    socklen_t &len_ref() { return len_; }

    /**
     * @brief Gets the port number (for INET/INET6 addresses)
     * @return Port number in host byte order
     */
    uint16_t port() const;

    /**
     * @brief Sets the port number (for INET/INET6 addresses)
     * @param port Port number in host byte order
     * @return Reference to this object for chaining
     */
    sock_addr& set_port(uint16_t port);

    /**
     * @brief Gets the address family
     * @return Address family (AF_INET, AF_INET6, or AF_UNIX)
     */
    uint16_t family() const;

    /**
     * @brief Gets the socket type
     * @return Socket type (e.g., SOCK_STREAM, SOCK_DGRAM)
     */
    uint16_t type() const;

    /**
     * @brief Gets the protocol
     * @return Protocol number
     */
    uint16_t protocol() const;

    /**
     * @brief Converts the address to its string representation
     * @return String representation of the address
     */
    std::string to_string() const;

    /**
     * @brief Gets the IP address as a string without port
     * @return String representation of just the address
     */
    std::string address_to_string() const;

    /**
     * @brief Retrieves a string representation with prefix notation
     * @return String with CIDR prefix if applicable
     */
    std::string to_cidr_string() const;

    /**
     * @brief Gets the network prefix length
     * @return Network prefix length (0-32 for IPv4, 0-128 for IPv6)
     */
    uint8_t prefix() const { return prefix_; }

    /**
     * @brief Equality comparison operator
     * @param other Address to compare with
     * @return true if addresses are equal
     */
    bool operator==(const sock_addr& other) const noexcept;

    /**
     * @brief Inequality comparison operator
     * @param other Address to compare with
     * @return true if addresses are not equal
     */
    bool operator!=(const sock_addr& other) const noexcept;

    /**
     * @brief Less than comparison operator for ordering
     * @param other Address to compare with
     * @return true if this address is less than other
     */
    bool operator<(const sock_addr& other) const noexcept;

    /**
     * @brief Greater than comparison operator
     * @param other Address to compare with
     * @return true if this address is greater than other
     */
    bool operator>(const sock_addr& other) const noexcept;

    /**
     * @brief Less than or equal comparison operator
     * @param other Address to compare with
     * @return true if this address is less than or equal to other
     */
    bool operator<=(const sock_addr& other) const noexcept;

    /**
     * @brief Greater than or equal comparison operator
     * @param other Address to compare with
     * @return true if this address is greater than or equal to other
     */
    bool operator>=(const sock_addr& other) const noexcept;

    /**
     * @brief Boolean conversion operator
     * @return true if the address is properly set (has non-zero length)
     *
     * This allows for constructions like:
     * @code
     * sock_addr addr("192.168.1.1:80");
     * if (addr) {
     *     // Address is valid
     * }
     * @endcode
     */
    explicit operator bool() const noexcept;

private:
    /**
     * @brief Parses common address formats into sockaddr structure
     * @param address Address string to parse
     * @param family Address family
     * @param socktype Socket type
     * @param proto Protocol
     * @return true if parsing was successful
     */
    bool parse_into_sockaddr_common(const std::string_view &address, uint8_t family, uint16_t socktype, uint16_t proto);

    /**
     * @brief Parses Unix domain socket address
     * @param path Socket path
     * @param socktype Socket type
     * @return true if parsing was successful
     */
    bool parse_unix_address(const std::string_view &path, uint16_t socktype);

    /**
     * @brief Parses Internet address (IPv4/IPv6)
     * @param address Address string
     * @param family Address family
     * @param socktype Socket type
     * @param proto Protocol
     * @return true if parsing was successful
     */
    bool parse_inet_address(const std::string_view &address, uint8_t family, uint16_t socktype, uint16_t proto);

    /**
     * @brief Sets up a wildcard address
     * @param port Port string or nullptr
     * @param family Address family
     * @param socktype Socket type
     * @param proto Protocol
     * @return true if setup was successful
     */
    bool setup_wildcard(const char* port, uint8_t family, uint16_t socktype, uint16_t proto);

    /**
     * @brief Setup localhost address (127.0.0.1 or ::1)
     * @param port Optional port number string
     * @param family Address family (AF_INET, AF_INET6, or AF_UNSPEC)
     * @param socktype Socket type
     * @param proto Protocol
     * @return true if setup was successful
     */
    bool setup_localhost(const char* port, uint8_t family, uint16_t socktype, uint16_t proto);

    socklen_t len_;     ///< Length of the address structure
    union {             ///< Union of different address types
        struct sockaddr sa;
        struct sockaddr_in sin;
        struct sockaddr_in6 sin6;
        struct sockaddr_un sun;
    } addr_;
    uint16_t type_;     ///< Socket type
    uint16_t protocol_; ///< Protocol number
    uint8_t prefix_;    ///< Network prefix length
    mutable std::string str_;    ///< String representation of the address
    mutable bool str_valid_{false}; ///< Flag to indicate if str_ is valid
};

/**
 * @brief Stream output operator for sock_addr
 * @param os Output stream
 * @param sa Socket address to output
 * @return Reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const sock_addr& sa);

/**
 * @brief IP network range checker for subnet operations
 *
 * This class represents an IP network range and provides efficient membership testing.
 * It supports both IPv4 and IPv6 networks with their respective prefix lengths.
 * Can be used for subnet matching, access control lists, and routing decisions.
 *
 * Example usage:
 * @code
 * // Create a network range for IPv4 subnet
 * network_range private_net("192.168.1.0/24");
 * if (private_net.valid()) {
 *     // Test if addresses belong to the subnet
 *     sock_addr client1("192.168.1.10", AF_INET);
 *     sock_addr client2("192.168.2.1", AF_INET);
 *     
 *     bool in_subnet1 = private_net.contains(client1);  // true
 *     bool in_subnet2 = private_net.contains(client2);  // false
 *     
 *     std::cout << "Network: " << private_net.network().to_string() << std::endl;
 *     std::cout << "Prefix: " << static_cast<int>(private_net.prefix()) << std::endl;
 * }
 *
 * // IPv6 subnet example
 * network_range ipv6_net("[2001:db8::]/64");
 * if (ipv6_net.valid()) {
 *     sock_addr addr("[2001:db8::1]", AF_INET6);
 *     bool in_subnet = ipv6_net.contains(addr);  // true
 * }
 *
 * // Allow all IPv4 addresses
 * network_range any("0.0.0.0/0");
 * @endcode
 */
class network_range {
public:
    /**
     * @brief Default constructor
     */
    network_range() noexcept;

    /**
     * @brief Construct from a sock_addr with its prefix
     * @param addr Base network address with prefix
     */
    explicit network_range(const sock_addr& addr) noexcept;

    /**
     * @brief Construct from a CIDR or netmask string
     * 
     * Supports the following formats:
     * - CIDR notation: "192.168.1.0/24", "[2001:db8::]/64"
     * - IPv4 with netmask: "192.168.1.0/255.255.255.0"
     * 
     * @param addr_str Address string in supported format
     * @param family Optional address family (AF_INET, AF_INET6, or AF_UNSPEC)
     * 
     * Example:
     * @code
     * network_range mask1("192.168.1.0/24");           // IPv4 CIDR
     * network_range mask2("192.168.1.0/255.255.255.0"); // IPv4 with netmask
     * network_range mask3("[2001:db8::]/64");          // IPv6 CIDR
     * @endcode
     */
    explicit network_range(const std::string& addr_str, uint16_t family = AF_UNSPEC) noexcept;

    /**
     * @brief Check if an address belongs to this network
     * @param addr Address to check
     * @return true if address is in the network
     */
    bool contains(const sock_addr& addr) const noexcept;

    /**
     * @brief Get the network address
     */
    const sock_addr& network() const noexcept { return network_; }

    /**
     * @brief Get the prefix length
     */
    uint8_t prefix() const noexcept { return prefix_; }

    /**
     * @brief Check if the range is valid
     */
    bool valid() const noexcept { return valid_; }

    /**
     * @brief Convert the network range to a string representation
     * @return CIDR notation as string
     */
    std::string to_string() const noexcept;

private:
    sock_addr network_;    ///< Network address
    uint8_t prefix_;       ///< Prefix length
    bool valid_;          ///< Validity flag

    // Pre-computed masks for quick matching
    union {
        uint32_t v4;      ///< IPv4 netmask
        uint8_t v6[16];   ///< IPv6 netmask
    } mask_;

    void init_mask() noexcept;
    void apply_mask() noexcept;

    /**
     * @brief Parse netmask string and convert to prefix length
     * @param mask_str Netmask string (e.g., "255.255.255.0")
     * @return Prefix length or 0 if invalid
     */
    static uint8_t netmask_to_prefix(const std::string& mask_str) noexcept;
};

} // namespace io

namespace std {
/**
 * @brief Hash function specialization for sock_addr
 * 
 * Enables using sock_addr as key in unordered containers
 */
template<>
struct hash<io::sock_addr> {
    /**
     * @brief Calculate hash value for sock_addr
     * @param addr Socket address to hash
     * @return Hash value
     */
    size_t operator()(const io::sock_addr& addr) const noexcept;
};
} // namespace std