#pragma once

#include <net/def/sockaddr.hpp>

namespace io
{

sock_addr::sock_addr() : len_(0), type_(0), protocol_(0), prefix_(0), str_valid_(false) { memset(&addr_, 0, sizeof(addr_)); }

sock_addr::sock_addr(const struct sockaddr_in &sa, uint8_t prefix, uint16_t socktype, uint16_t proto)
: len_(0),
  type_(socktype),
  protocol_(proto),
  prefix_(prefix),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin = sa;
    len_      = sizeof(sa); // Set length only after successful initialization
}

sock_addr::sock_addr(const struct sockaddr_in6 &sa, uint8_t prefix, uint16_t socktype, uint16_t proto)
: len_(0),
  type_(socktype),
  protocol_(proto),
  prefix_(prefix),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6 = sa;
    len_       = sizeof(sa); // Set length only after successful initialization
}

sock_addr::sock_addr(const struct sockaddr_un &sa, uint16_t socktype)
: len_(0),
  type_(socktype),
  protocol_(0),
  prefix_(0),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun = sa;
    len_      = sizeof(sa); // Set length only after successful initialization
}

sock_addr::sock_addr(const struct sockaddr *sa, socklen_t len, uint8_t prefix, uint16_t socktype, uint16_t proto)
: len_(0),
  type_(socktype),
  protocol_(proto),
  prefix_(prefix),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    if (!sa) return;

    switch (sa->sa_family)
    {
    case AF_INET:
        if (len != sizeof(struct sockaddr_in)) { return; }
        memcpy(&addr_.sin, sa, sizeof(struct sockaddr_in));
        len_ = len;
        if (prefix == 0) { prefix_ = 32; }
        break;
    case AF_INET6:
        if (len != sizeof(struct sockaddr_in6)) { return; }
        memcpy(&addr_.sin6, sa, sizeof(struct sockaddr_in6));
        len_ = len;
        if (prefix == 0) { prefix_ = 128; }
        break;
    case AF_UNIX:
        addr_.sun = *reinterpret_cast<const struct sockaddr_un *>(sa);
        len_      = len;
        break;
    default: return;
    }
}

sock_addr::sock_addr(const std::string &addrfull, uint16_t family, uint16_t socktype, uint16_t proto)
: len_(0),
  type_(socktype),
  protocol_(proto),
  prefix_(0),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    parse_into_sockaddr_common(addrfull, family, socktype, proto);
}

sock_addr::sock_addr(const std::string_view &addr, const std::string_view &port, uint16_t family, uint16_t socktype, uint16_t proto)
: len_(0),
  type_(socktype),
  protocol_(proto),
  prefix_(0),
  str_valid_(false)
{
    memset(&addr_, 0, sizeof(addr_));
    std::string addr_str;
    
    // Handle IPv6 address - needs brackets
    if (family == AF_INET6) {
        // Check if the address already has brackets
        if (addr.size() >= 2 && addr[0] == '[' && addr[addr.size()-1] == ']') {
            addr_str = std::string(addr) + ":" + std::string(port);
        } else {
            // Add brackets around the IPv6 address
            addr_str = "[" + std::string(addr) + "]:" + std::string(port);
        }
    } else {
        // IPv4 or other address type
        addr_str = std::string(addr) + ":" + std::string(port);
    }
    
    parse_into_sockaddr_common(addr_str, family, socktype, proto);
}

sock_addr::sock_addr(sock_addr&& other) noexcept
    : len_(other.len_),
      addr_(other.addr_),
      type_(other.type_),
      protocol_(other.protocol_),
      prefix_(other.prefix_),
      str_(std::move(other.str_)),
      str_valid_(other.str_valid_)
{
    // Reset the source object
    other.len_ = 0;
    other.type_ = 0;
    other.protocol_ = 0;
    other.prefix_ = 0;
    other.str_valid_ = false;
    memset(&other.addr_, 0, sizeof(other.addr_));
}

sock_addr& sock_addr::operator=(sock_addr&& other) noexcept
{
    if (this != &other) {
        len_ = other.len_;
        addr_ = other.addr_;
        type_ = other.type_;
        protocol_ = other.protocol_;
        prefix_ = other.prefix_;
        str_ = std::move(other.str_);
        str_valid_ = other.str_valid_;

        // Reset the source object
        other.len_ = 0;
        other.type_ = 0;
        other.protocol_ = 0;
        other.prefix_ = 0;
        other.str_valid_ = false;
        memset(&other.addr_, 0, sizeof(other.addr_));
    }
    return *this;
}

sock_addr::sock_addr(const sock_addr& other) noexcept
    : len_(other.len_),
      addr_(other.addr_),
      type_(other.type_),
      protocol_(other.protocol_),
      prefix_(other.prefix_),
      str_(other.str_),
      str_valid_(other.str_valid_)
{
}

sock_addr& sock_addr::operator=(const sock_addr& other) noexcept
{
    if (this != &other) {
        len_ = other.len_;
        addr_ = other.addr_;
        type_ = other.type_;
        protocol_ = other.protocol_;
        prefix_ = other.prefix_;
        str_ = other.str_;
        str_valid_ = other.str_valid_;
    }
    return *this;
}

std::ostream &operator<<(std::ostream &os, const sock_addr &sa)
{
    os << sa.to_string();
    return os;
}

bool sock_addr::setup_wildcard(const char *port, uint8_t family, uint16_t socktype, uint16_t proto)
{
    memset(&addr_, 0, sizeof(addr_));
    type_     = socktype;
    protocol_ = proto;

    if (family == AF_INET || family == AF_UNSPEC)
    {
        addr_.sin.sin_family      = AF_INET;
        addr_.sin.sin_addr.s_addr = INADDR_ANY;
        addr_.sin.sin_port        = port ? htons(std::stoi(port)) : 0;
        len_                      = sizeof(struct sockaddr_in);
        prefix_                   = 32;
        return true;
    }
    else if (family == AF_INET6)
    {
        addr_.sin6.sin6_family = AF_INET6;
        addr_.sin6.sin6_addr   = in6addr_any;
        addr_.sin6.sin6_port   = port ? htons(std::stoi(port)) : 0;
        len_                   = sizeof(struct sockaddr_in6);
        prefix_                = 128;
        return true;
    }
    return false;
}

bool sock_addr::setup_localhost(const char *port, uint8_t family, uint16_t socktype, uint16_t proto)
{
    memset(&addr_, 0, sizeof(addr_));
    type_     = socktype;
    protocol_ = proto;

    if (family == AF_INET || family == AF_UNSPEC)
    {
        addr_.sin.sin_family      = AF_INET;
        addr_.sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_.sin.sin_port        = port ? htons(std::stoi(port)) : 0;
        len_                      = sizeof(struct sockaddr_in);
        prefix_                   = 32;
        return true;
    }
    else if (family == AF_INET6)
    {
        addr_.sin6.sin6_family = AF_INET6;
        addr_.sin6.sin6_addr   = in6addr_loopback;
        addr_.sin6.sin6_port   = port ? htons(std::stoi(port)) : 0;
        len_                   = sizeof(struct sockaddr_in6);
        prefix_                = 128;
        return true;
    }
    return false;
}

bool sock_addr::parse_into_sockaddr_common(const std::string_view &address, uint8_t family, uint16_t socktype, uint16_t proto)
{
    // For Unix domain sockets, handle directly without getaddrinfo
    if (family == AF_UNIX) { return parse_unix_address(address, socktype); }

    return parse_inet_address(address, family, socktype, proto);
}

bool sock_addr::parse_inet_address(const std::string_view &address, uint8_t family, uint16_t socktype, uint16_t proto)
{
    if (address.empty()) { return false; }

    // Replace VLA with std::vector
    std::vector<char> addr_copy(address.size() + 1);
    std::memcpy(addr_copy.data(), address.data(), address.size());
    addr_copy[address.size()] = '\0';
    char *addr                = addr_copy.data();

    char port[6]             = {0};
    uint8_t prefix           = 0;
    bool has_port            = false;
    bool has_prefix          = false;
    bool error_cuz_of_braket = false;

    // Special handling for common cases
    if (strcmp(addr, "localhost") == 0) { 
        return setup_localhost(nullptr, family, socktype, proto); 
    }
    
    // Check for wildcards before processing them
    // This fix handles cases like "*:8080" properly by extracting the port first
    bool is_wildcard = (*addr == '*' || strcmp(addr, "any") == 0);
    
    // Handle brackets for IPv6
    if (*addr == '[')
    {
        addr++;
        error_cuz_of_braket = true;
    }

    // Find different parts of the address string
    char *bracket_end  = strchr(addr, ']');
    char *port_start   = strchr(bracket_end ? bracket_end : addr, ':');
    char *prefix_start = strchr(bracket_end ? bracket_end : addr, '/');

    // Handle IPv6 bracket closure
    if (bracket_end)
    {
        error_cuz_of_braket = false;
        *bracket_end        = '\0';
        // Adjust port/prefix pointers if they're before the bracket end
        if (port_start && port_start < bracket_end) port_start = strchr(bracket_end + 1, ':');
        if (prefix_start && prefix_start < bracket_end) prefix_start = strchr(bracket_end + 1, '/');
    }

    // Process prefix if present
    if (prefix_start)
    {
        char *prefix_str = prefix_start + 1;
        char *endptr;
        unsigned long ival = strtoul(prefix_str, &endptr, 10);

        if (endptr == prefix_str || (*endptr != '\0' && *endptr != ':'))
        {
            LOG(error).print("Invalid prefix format");
            return false;
        }

        prefix     = ival;
        has_prefix = true;

        // If there's a port after the prefix
        if (*endptr == ':') { port_start = endptr; }
        *prefix_start = '\0';
    }

    // Process port if present before handling wildcards
    if (port_start)
    {
        const char *port_str = port_start + 1;
        try {
            int port_num = std::stoi(port_str);
            if (port_num < 0 || port_num > 65535) {
                LOG(error).print("Port number out of range");
                return false;
            }
            std::snprintf(port, sizeof(port), "%d", port_num);
            has_port = true;
        } catch (const std::exception& e) {
            LOG(error).printf("Invalid port number: %s", e.what());
            return false;
        }
        *port_start = '\0';
    }

    // Now handle wildcards after port is processed
    if (is_wildcard) { 
        return setup_wildcard(has_port ? port : nullptr, family, socktype, proto); 
    }

    if (error_cuz_of_braket)
    {
        LOG(error).print("Bad IP string, terminating ']' not found");
        return false;
    }

    // Check for localhost before wildcard
    if (strcmp(addr, "localhost") == 0) { return setup_localhost(has_port ? port : nullptr, family, socktype, proto); }

    // Handle wildcard addresses
    if (*addr == '*' || strncmp(addr, "any", 3) == 0)
    {
        return setup_wildcard(has_port ? port : nullptr, family, socktype, proto);
    }

    // Setup and call getaddrinfo
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = proto;
    hints.ai_flags    = AI_NUMERICHOST | AI_NUMERICSERV;

    int error;
    if ((error = getaddrinfo(addr, (has_port ? port : NULL), &hints, &res)) == 0)
    {
        if (res->ai_addrlen > sizeof(addr_))
        {
            LOG(error).print("sockaddr too large");
            freeaddrinfo(res);
            return false;
        }

        // Copy address and validate prefix
        memcpy(&addr_, res->ai_addr, res->ai_addrlen);
        len_      = res->ai_addrlen;
        type_     = res->ai_socktype;
        protocol_ = res->ai_protocol;

        // Now that we know the actual address family, validate the prefix
        if (has_prefix)
        {
            uint8_t max_prefix = (addr_.sa.sa_family == AF_INET6) ? 128 : 32;
            if (prefix > max_prefix)
            {
                LOG(error).print("Prefix out of range for address family");
                freeaddrinfo(res);
                len_ = 0;
                return false;
            }
            prefix_ = prefix;
        }
        else { prefix_ = (addr_.sa.sa_family == AF_INET) ? 32 : 128; }

        freeaddrinfo(res);
        return true;
    }

    LOG(error).printf("getaddrinfo failed: %s", gai_strerror(error));
    return false;
}

bool sock_addr::parse_unix_address(const std::string_view &path, uint16_t socktype)
{
    memset(&addr_, 0, sizeof(addr_));
    len_                 = 0; // Initialize to 0 first
    addr_.sun.sun_family = AF_UNIX;
    type_                = socktype;
    protocol_            = 0;
    prefix_              = 0;

    // Handle abstract socket paths (Linux-specific)
    if (!path.empty() && path[0] == '@')
    {
        addr_.sun.sun_path[0] = '\0';
        const size_t copy_len = std::min(path.length() - 1, sizeof(addr_.sun.sun_path) - 1);
        memcpy(addr_.sun.sun_path + 1, path.data() + 1, copy_len);
        len_ = sizeof(sa_family_t) + copy_len + 1;
        return true;
    }

    // Regular Unix domain socket path
    if (path.length() >= sizeof(addr_.sun.sun_path))
    {
        LOG(error).print("Unix domain socket path too long");
        len_ = 0;
        return false;
    }

    strncpy(addr_.sun.sun_path, path.data(), sizeof(addr_.sun.sun_path) - 1);
    addr_.sun.sun_path[sizeof(addr_.sun.sun_path) - 1] = '\0';
    len_                                               = sizeof(struct sockaddr_un);
    return true;
}

struct sockaddr *sock_addr::sockaddr() { return reinterpret_cast<struct sockaddr *>(&addr_); }

const struct sockaddr *sock_addr::sockaddr() const { return reinterpret_cast<const struct sockaddr *>(&addr_); }

socklen_t sock_addr::len() const { return len_; }

uint16_t sock_addr::port() const
{
    return ntohs(addr_.sa.sa_family == AF_INET ? addr_.sin.sin_port : addr_.sin6.sin6_port);
}

uint16_t sock_addr::family() const { return addr_.sa.sa_family; }

uint16_t sock_addr::type() const { return type_; }

uint16_t sock_addr::protocol() const { return protocol_; }

std::string sock_addr::to_string() const
{
    if (str_valid_) return str_;
    
    std::string str;
    switch (addr_.sa.sa_family)
    {
    case AF_INET:
        char addr4[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr_.sin.sin_addr, addr4, INET_ADDRSTRLEN)) {
            str = addr4;
            if (port()) str += ":" + std::to_string(port());
        } else {
            str = "invalid-ipv4";
        }
        break;
    case AF_INET6:
        char addr6[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &addr_.sin6.sin6_addr, addr6, INET6_ADDRSTRLEN)) {
            str = "[";
            str += addr6;
            str += "]";
            if (port()) str += ":" + std::to_string(port());
        } else {
            str = "invalid-ipv6";
        }
        break;
    case AF_UNIX:
        if (addr_.sun.sun_path[0] == '\0') {
            // Handle abstract socket
            str = "@";
            // Calculate the length of the abstract name
            size_t abstract_len = len_ - sizeof(sa_family_t) - 1;
            if (abstract_len > 0) {
                str.append(addr_.sun.sun_path + 1, abstract_len);
            }
        } else {
            str = addr_.sun.sun_path;
        }
        break;
    default: str = "Unknown address family"; break; // Handle unknown address families
    }
    
    str_ = str;
    str_valid_ = true;
    return str;
}

sock_addr& sock_addr::set_port(uint16_t port) {
    if (addr_.sa.sa_family == AF_INET) {
        addr_.sin.sin_port = htons(port);
        str_valid_ = false;
    } else if (addr_.sa.sa_family == AF_INET6) {
        addr_.sin6.sin6_port = htons(port);
        str_valid_ = false;
    }
    return *this;
}

std::string sock_addr::address_to_string() const {
    switch (addr_.sa.sa_family) {
    case AF_INET: {
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr_.sin.sin_addr, addr, INET_ADDRSTRLEN);
        return std::string(addr);
    }
    case AF_INET6: {
        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr_.sin6.sin6_addr, addr, INET6_ADDRSTRLEN);
        return std::string(addr);
    }
    case AF_UNIX:
        // Same as to_string() for Unix domain sockets
        return to_string();
    default:
        return "Unknown address family";
    }
}

std::string sock_addr::to_cidr_string() const {
    if ((addr_.sa.sa_family != AF_INET && addr_.sa.sa_family != AF_INET6) || prefix_ == 0) {
        return to_string();
    }
    
    std::string base = to_string();
    std::string prefix_str = "/" + std::to_string(prefix_);
    
    if (addr_.sa.sa_family == AF_INET) {
        // For IPv4: "addr/prefix[:port]"
        size_t port_pos = base.rfind(':');
        if (port_pos != std::string::npos) {
            // Insert prefix before port
            return base.substr(0, port_pos) + prefix_str + base.substr(port_pos);
        } else {
            // No port, just append prefix
            return base + prefix_str;
        }
    } else {
        // For IPv6: "[addr]/prefix[:port]"
        size_t bracket_pos = base.rfind(']');
        if (bracket_pos == std::string::npos) {
            // Should not happen with valid IPv6 addresses
            return base + prefix_str;
        }
        
        // Check if there's a port after the bracket
        size_t port_pos = base.find(':', bracket_pos);
        if (port_pos != std::string::npos) {
            // Insert prefix between bracket and port
            return base.substr(0, bracket_pos + 1) + prefix_str + base.substr(port_pos);
        } else {
            // No port, append after bracket
            return base.substr(0, bracket_pos + 1) + prefix_str;
        }
    }
}

bool sock_addr::operator==(const sock_addr &other) const noexcept
{
    if (addr_.sa.sa_family != other.addr_.sa.sa_family) return false;

    switch (addr_.sa.sa_family)
    {
    case AF_INET:
        return addr_.sin.sin_addr.s_addr == other.addr_.sin.sin_addr.s_addr && addr_.sin.sin_port == other.addr_.sin.sin_port;
    case AF_INET6:
        return memcmp(&addr_.sin6.sin6_addr, &other.addr_.sin6.sin6_addr, sizeof(struct in6_addr)) == 0 &&
               addr_.sin6.sin6_port == other.addr_.sin6.sin6_port;
    case AF_UNIX:
        if (addr_.sun.sun_path[0] == '\0')
        {
            // Abstract socket - compare actual length for abstract sockets
            return len_ == other.len_ && memcmp(addr_.sun.sun_path, other.addr_.sun.sun_path, len_ - sizeof(sa_family_t)) == 0;
        }
        return strcmp(addr_.sun.sun_path, other.addr_.sun.sun_path) == 0;
    default: return false;
    }
}

bool sock_addr::operator!=(const sock_addr& other) const noexcept { 
    return !(*this == other); 
}

bool sock_addr::operator>(const sock_addr& other) const noexcept { 
    return other < *this; 
}

bool sock_addr::operator<=(const sock_addr& other) const noexcept { 
    return !(*this > other); 
}

bool sock_addr::operator>=(const sock_addr& other) const noexcept { 
    return !(*this < other); 
}

bool sock_addr::operator<(const sock_addr &other) const noexcept
{
    // First compare by address family
    if (addr_.sa.sa_family != other.addr_.sa.sa_family) return addr_.sa.sa_family < other.addr_.sa.sa_family;

    switch (addr_.sa.sa_family)
    {
    case AF_INET:
        // Compare IPv4 addresses
        if (addr_.sin.sin_addr.s_addr != other.addr_.sin.sin_addr.s_addr)
            return ntohl(addr_.sin.sin_addr.s_addr) < ntohl(other.addr_.sin.sin_addr.s_addr);
        return ntohs(addr_.sin.sin_port) < ntohs(other.addr_.sin.sin_port);

    case AF_INET6:
        // Compare IPv6 addresses
        {
            int cmp = memcmp(&addr_.sin6.sin6_addr, &other.addr_.sin6.sin6_addr, sizeof(struct in6_addr));
            if (cmp != 0) return cmp < 0;
            return ntohs(addr_.sin6.sin6_port) < ntohs(other.addr_.sin6.sin6_port);
        }

    case AF_UNIX:
        // Handle abstract sockets (Linux-specific)
        if (addr_.sun.sun_path[0] == '\0')
        {
            // If lengths differ, compare lengths
            if (len_ != other.len_) return len_ < other.len_;
            // Compare the actual content including the leading null byte
            return memcmp(addr_.sun.sun_path, other.addr_.sun.sun_path, len_ - sizeof(sa_family_t)) < 0;
        }
        // Regular Unix domain socket paths
        return strcmp(addr_.sun.sun_path, other.addr_.sun.sun_path) < 0;

    default: return false;
    }
}

sock_addr::operator bool() const noexcept 
{
    return len_ > 0;
}

network_range::network_range() noexcept : prefix_(0), valid_(false) { memset(&mask_, 0, sizeof(mask_)); }

network_range::network_range(const sock_addr &addr) noexcept
: network_(addr),
  prefix_(addr.prefix()),
  valid_(false) // Start with invalid
{
    memset(&mask_, 0, sizeof(mask_));

    // Check for invalid or empty address
    if (network_.len() == 0) { return; }

    // Only handle IPv4 and IPv6
    switch (network_.family())
    {
    case AF_INET:
        if (prefix_ > 32) { return; }
        break;
    case AF_INET6:
        if (prefix_ > 128) { return; }
        break;
    default: return;
    }

    init_mask();
    apply_mask();
    valid_ = true;
}

network_range::network_range(const std::string &addr_str, uint16_t family) noexcept : prefix_(0), valid_(false)
{
    memset(&mask_, 0, sizeof(mask_));

    size_t slash_pos = addr_str.find('/');
    if (slash_pos == std::string::npos) { return; }

    std::string addr_part = addr_str.substr(0, slash_pos);
    std::string mask_part = addr_str.substr(slash_pos + 1);

    // Try to parse as CIDR first
    char *endptr;
    unsigned long prefix = strtoul(mask_part.c_str(), &endptr, 10);
    if (*endptr == '\0')
    {
        // It's a CIDR prefix
        network_ = sock_addr(addr_part, family);
        if (network_.len() == 0) { return; }
        prefix_ = prefix;
    }
    else if (family != AF_INET6)
    {
        // Try as IPv4 netmask
        prefix_ = netmask_to_prefix(mask_part);
        if (prefix_ == 0) { return; }
        network_ = sock_addr(addr_part, AF_INET);
        if (network_.len() == 0) { return; }
    }
    else { return; }

    // Validate prefix for the address family
    switch (network_.family())
    {
    case AF_INET:
        if (prefix_ > 32) { return; }
        break;
    case AF_INET6:
        if (prefix_ > 128) { return; }
        break;
    default: return;
    }

    init_mask();
    apply_mask();
    valid_ = true;
}

uint8_t network_range::netmask_to_prefix(const std::string &mask_str) noexcept
{
    sock_addr mask(mask_str, AF_INET);
    if (mask.len() == 0) { return 0; }

    uint32_t netmask = ntohl(reinterpret_cast<const sockaddr_in *>(mask.sockaddr())->sin_addr.s_addr);

    // Validate that it's a valid netmask (continuous 1s followed by continuous 0s)
    uint32_t check = ~netmask + 1;
    if (check & (check - 1)) { return 0; }

    // Count the 1s
    uint8_t prefix = 0;
    while (netmask & (1U << 31))
    {
        prefix++;
        netmask <<= 1;
    }

    // Ensure there are no more 1s
    if (netmask != 0) { return 0; }

    return prefix;
}

void network_range::init_mask() noexcept
{
    if (network_.family() == AF_INET)
    {
        // For IPv4, create a 32-bit mask
        prefix_  = std::min(prefix_, uint8_t(32));
        mask_.v4 = prefix_ ? htonl(0xFFFFFFFF << (32 - prefix_)) : 0;
    }
    else if (network_.family() == AF_INET6)
    {
        // For IPv6, create a 128-bit mask
        prefix_ = std::min(prefix_, uint8_t(128));

        const int full_bytes     = prefix_ / 8;
        const int remaining_bits = prefix_ % 8;

        // Fill complete bytes with 1s
        if (full_bytes > 0) { memset(mask_.v6, 0xFF, full_bytes); }

        // Handle partial byte
        if (remaining_bits > 0) { mask_.v6[full_bytes] = uint8_t(0xFF << (8 - remaining_bits)); }

        // Zero remaining bytes
        if (full_bytes < 16)
        {
            memset(mask_.v6 + full_bytes + (remaining_bits ? 1 : 0), 0, 16 - full_bytes - (remaining_bits ? 1 : 0));
        }
    }
}

void network_range::apply_mask() noexcept
{
    if (network_.family() == AF_INET)
    {
        auto &sin = reinterpret_cast<struct sockaddr_in &>(*network_.sockaddr());
        sin.sin_addr.s_addr &= mask_.v4;
        sin.sin_port = 0;
    }
    else if (network_.family() == AF_INET6)
    {
        auto &sin6 = reinterpret_cast<struct sockaddr_in6 &>(*network_.sockaddr());
        for (int i = 0; i < 16; ++i) { sin6.sin6_addr.s6_addr[i] &= mask_.v6[i]; }
        sin6.sin6_port = 0;
    }
}

bool network_range::contains(const sock_addr &addr) const noexcept
{
    if (!valid_ || addr.family() != network_.family()) { return false; }

    if (network_.family() == AF_INET)
    {
        const auto &test_addr = reinterpret_cast<const struct sockaddr_in &>(*addr.sockaddr());
        const auto &net_addr  = reinterpret_cast<const struct sockaddr_in &>(*network_.sockaddr());

        return (test_addr.sin_addr.s_addr & mask_.v4) == net_addr.sin_addr.s_addr;
    }
    else if (network_.family() == AF_INET6)
    {
        const auto &test_addr = reinterpret_cast<const struct sockaddr_in6 &>(*addr.sockaddr());
        const auto &net_addr  = reinterpret_cast<const struct sockaddr_in6 &>(*network_.sockaddr());

        for (int i = 0; i < 16; ++i)
        {
            if ((test_addr.sin6_addr.s6_addr[i] & mask_.v6[i]) != net_addr.sin6_addr.s6_addr[i]) { return false; }
        }
        return true;
    }

    return false;
}

std::string network_range::to_string() const noexcept {
    if (!valid_) return "invalid-network";
    
    std::string result;
    if (network_.family() == AF_INET) {
        result = network_.address_to_string() + "/" + std::to_string(prefix_);
    } else if (network_.family() == AF_INET6) {
        result = "[" + network_.address_to_string() + "]/" + std::to_string(prefix_);
    }
    return result;
}

} // namespace io

namespace std
{
size_t hash<io::sock_addr>::operator()(const io::sock_addr &addr) const noexcept
{
    size_t h = 0;
    switch (addr.family())
    {
    case AF_INET:
    {
        const auto &sin = reinterpret_cast<const sockaddr_in &>(*addr.sockaddr());
        h               = std::hash<uint32_t>{}(sin.sin_addr.s_addr);
        // Combine the port hash with the address hash using a variant of FNV-1a hash algorithm
        // 0x9e3779b9 is derived from the golden ratio and helps distribute hash values
        h ^= std::hash<uint16_t>{}(sin.sin_port) + 0x9e3779b9 + (h << 6) + (h >> 2);
        break;
    }
    case AF_INET6:
    {
        const auto &sin6          = reinterpret_cast<const sockaddr_in6 &>(*addr.sockaddr());
        const uint8_t *addr_bytes = sin6.sin6_addr.s6_addr;
        for (size_t i = 0; i < 16; ++i) { h ^= std::hash<uint8_t>{}(addr_bytes[i]) + 0x9e3779b9 + (h << 6) + (h >> 2); }
        h ^= std::hash<uint16_t>{}(sin6.sin6_port) + 0x9e3779b9 + (h << 6) + (h >> 2);
        break;
    }
    case AF_UNIX:
    {
        const auto &sun = reinterpret_cast<const sockaddr_un &>(*addr.sockaddr());
        if (sun.sun_path[0] == '\0')
        {
            h                = std::hash<size_t>{}(addr.len());
            const char *path = sun.sun_path;
            for (size_t i = 0; i < addr.len() - sizeof(sa_family_t); ++i)
            {
                h ^= std::hash<char>{}(path[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
        }
        else { h = std::hash<std::string>{}(sun.sun_path); }
        break;
    }
    }
    return h;
}

} // namespace std