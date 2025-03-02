#include <io/iotask.hpp>
#include <common/catch.hpp>
#include <io/sockaddr.hpp>
#include <sys/un.h>

using namespace io;

TEST_CASE("basic sock_addr", "[sock_addr]") {
    SECTION("create and check size") {
        sock_addr addr;
        REQUIRE(addr.len() == 0);
        REQUIRE(addr.family() == 0);
        REQUIRE(addr.type() == 0);
        REQUIRE(addr.protocol() == 0);
        REQUIRE(addr.port() == 0);
    }
}

TEST_CASE("ipv4 sock_addr", "[sock_addr]") {
    SECTION("ipv4 tcp") {
        sock_addr addr("127.0.0.1:8080", AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(addr.len() == sizeof(struct sockaddr_in));
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.type() == SOCK_STREAM);
        REQUIRE(addr.protocol() == IPPROTO_TCP);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "127.0.0.1:8080");
    }

    SECTION("ipv4 udp") {
        sock_addr addr("0.0.0.0:53", AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.type() == SOCK_DGRAM);
        REQUIRE(addr.protocol() == IPPROTO_UDP);
        REQUIRE(addr.port() == 53);
        REQUIRE(addr.to_string() == "0.0.0.0:53");
    }

    SECTION("ipv4 without port") {
        sock_addr addr("192.168.1.1", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.port() == 0);
        REQUIRE(addr.to_string() == "192.168.1.1");
    }
}

TEST_CASE("ipv6 sock_addr", "[sock_addr]") {
    SECTION("ipv6 tcp") {
        sock_addr addr("[::1]:8080", AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(addr.len() == sizeof(struct sockaddr_in6));
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.type() == SOCK_STREAM);
        REQUIRE(addr.protocol() == IPPROTO_TCP);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "[::1]:8080");
    }

    SECTION("ipv6 udp") {
        sock_addr addr("[::]:53", AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.type() == SOCK_DGRAM);
        REQUIRE(addr.protocol() == IPPROTO_UDP);
        REQUIRE(addr.port() == 53);
        REQUIRE(addr.to_string() == "[::]:53");
    }

    SECTION("ipv6 without port") {
        sock_addr addr("[2001:db8::1]", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.port() == 0);
        REQUIRE(addr.to_string() == "[2001:db8::1]");
    }
}

TEST_CASE("unix domain sock_addr", "[sock_addr]") {
    SECTION("unix stream") {
        sock_addr addr("/tmp/test.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.type() == SOCK_STREAM);
        REQUIRE(addr.to_string() == "/tmp/test.sock");
    }

    SECTION("unix dgram") {
        sock_addr addr("/tmp/test.sock", AF_UNIX, SOCK_DGRAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.type() == SOCK_DGRAM);
        REQUIRE(addr.to_string() == "/tmp/test.sock");
    }

    SECTION("unix relative path") {
        sock_addr addr("./socket", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.to_string() == "./socket");
    }

    SECTION("unix path with max length") {
        std::string long_path(sizeof(sockaddr_un::sun_path) - 1, 'a');
        sock_addr addr(long_path, AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.to_string() == long_path);
    }

    SECTION("unix path with spaces") {
        sock_addr addr("/tmp/test socket.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.to_string() == "/tmp/test socket.sock");
    }

    SECTION("unix abstract socket") {
        sock_addr addr("@test-abstract", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        REQUIRE(addr.type() == SOCK_STREAM);
        // Abstract sockets have a null byte at the start
        const char* path = reinterpret_cast<const sockaddr_un*>(addr.sockaddr())->sun_path;
        REQUIRE(path[0] == '\0');
        REQUIRE(memcmp(path + 1, "test-abstract", 12) == 0);
    }

    SECTION("unix abstract socket with special chars") {
        sock_addr addr("@test/socket:123", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        const char* path = reinterpret_cast<const sockaddr_un*>(addr.sockaddr())->sun_path;
        REQUIRE(path[0] == '\0');
        REQUIRE(memcmp(path + 1, "test/socket:123", 14) == 0);
    }

    SECTION("unix abstract socket with maximum length") {
        // Maximum length for abstract socket is sizeof(sun_path) - 1
        std::string name(sizeof(sockaddr_un::sun_path) - 2, 'x');  // -2 for @ and null byte
        sock_addr addr("@" + name, AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        const char* path = reinterpret_cast<const sockaddr_un*>(addr.sockaddr())->sun_path;
        REQUIRE(path[0] == '\0');
        REQUIRE(memcmp(path + 1, name.c_str(), name.length()) == 0);
    }

    SECTION("unix abstract socket empty name") {
        sock_addr addr("@", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.family() == AF_UNIX);
        const char* path = reinterpret_cast<const sockaddr_un*>(addr.sockaddr())->sun_path;
        REQUIRE(path[0] == '\0');
        REQUIRE(path[1] == '\0');  // Verify the next byte is also null
    }

    SECTION("unix path too long") {
        std::string long_path(sizeof(sockaddr_un::sun_path) + 10, 'a');
        sock_addr addr(long_path, AF_UNIX, SOCK_STREAM);
        REQUIRE(addr.len() == 0);
    }
}

TEST_CASE("sock_addr error cases", "[sock_addr]") {
    SECTION("invalid ipv4") {
        sock_addr addr("256.256.256.256:8080", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.len() == 0);
    }

    SECTION("invalid ipv6") {
        sock_addr addr("[fe80:::1]:8080", AF_INET6, SOCK_STREAM, 0);  // Invalid IPv6 with double colons
        REQUIRE(addr.len() == 0);
    }

    SECTION("mismatched brackets") {
        sock_addr addr("[::1:8080", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.len() == 0);
    }

    SECTION("invalid port") {
        sock_addr addr("127.0.0.1:999999", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.len() == 0);
    }
}

TEST_CASE("wildcard addresses", "[sock_addr]") {
    SECTION("ipv4 any") {
        sock_addr addr("any", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.to_string() == "0.0.0.0");
    }

    SECTION("ipv4 any with port") {
        sock_addr addr("any:8080", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "0.0.0.0:8080");
    }

    SECTION("ipv4 asterisk") {
        sock_addr addr("*", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.to_string() == "0.0.0.0");
    }

    SECTION("ipv4 asterisk with port") {
        sock_addr addr("*:8080", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "0.0.0.0:8080");
    }

    SECTION("ipv6 any") {
        sock_addr addr("[any]", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.to_string() == "[::]");
    }

    SECTION("ipv6 asterisk") {
        sock_addr addr("[*]", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.to_string() == "[::]");
    }

    SECTION("ipv4 explicit wildcard") {
        sock_addr addr("0.0.0.0", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.to_string() == "0.0.0.0");
    }

    SECTION("ipv4 explicit wildcard with port") {
        sock_addr addr("0.0.0.0:8080", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "0.0.0.0:8080");
    }

    SECTION("ipv6 explicit wildcard") {
        sock_addr addr("[::]", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.to_string() == "[::]");
    }

    SECTION("ipv6 explicit wildcard with port") {
        sock_addr addr("[::]:8080", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.port() == 8080);
        REQUIRE(addr.to_string() == "[::]:8080");
    }
}

TEST_CASE("cidr prefix addresses", "[sock_addr]") {
    SECTION("ipv4 with prefix") {
        sock_addr addr("192.168.1.0/24", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET);
        REQUIRE(addr.to_string() == "192.168.1.0");
    }

    SECTION("ipv4 with invalid prefix") {
        sock_addr addr("192.168.1.0/33", AF_INET, SOCK_STREAM, 0);
        REQUIRE(addr.len() == 0);
    }

    SECTION("ipv6 with prefix") {
        sock_addr addr("[2001:db8::]/64", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.family() == AF_INET6);
        REQUIRE(addr.to_string() == "[2001:db8::]");
    }

    SECTION("ipv6 with invalid prefix") {
        sock_addr addr("[2001:db8::]/129", AF_INET6, SOCK_STREAM, 0);
        REQUIRE(addr.len() == 0);
    }

}

TEST_CASE("sock_addr comparisons", "[sock_addr]") {
    SECTION("equal ipv4 addresses") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        REQUIRE(addr1 == addr2);
        REQUIRE_FALSE(addr1 != addr2);
        REQUIRE_FALSE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE(addr1 >= addr2);
    }

    SECTION("different ipv4 addresses") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("127.0.0.2:8080", AF_INET, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }

    SECTION("different ipv4 ports") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("127.0.0.1:8081", AF_INET, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }

    SECTION("equal ipv6 addresses") {
        sock_addr addr1("[::1]:8080", AF_INET6, SOCK_STREAM);
        sock_addr addr2("[::1]:8080", AF_INET6, SOCK_STREAM);
        REQUIRE(addr1 == addr2);
        REQUIRE_FALSE(addr1 != addr2);
        REQUIRE_FALSE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE(addr1 >= addr2);
    }

    SECTION("different ipv6 addresses") {
        sock_addr addr1("[::1]:8080", AF_INET6, SOCK_STREAM);
        sock_addr addr2("[::2]:8080", AF_INET6, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }

    SECTION("different address families") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("[::1]:8080", AF_INET6, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);  // AF_INET < AF_INET6
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }

    SECTION("equal unix domain sockets") {
        sock_addr addr1("/tmp/test.sock", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("/tmp/test.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr1 == addr2);
        REQUIRE_FALSE(addr1 != addr2);
        REQUIRE_FALSE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE(addr1 >= addr2);
    }

    SECTION("different unix domain sockets") {
        sock_addr addr1("/tmp/test1.sock", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("/tmp/test2.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }

    SECTION("equal abstract unix sockets") {
        sock_addr addr1("@test", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("@test", AF_UNIX, SOCK_STREAM);
        REQUIRE(addr1 == addr2);
        REQUIRE_FALSE(addr1 != addr2);
        REQUIRE_FALSE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE(addr1 >= addr2);
    }

    SECTION("different abstract unix sockets") {
        sock_addr addr1("@test1", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("@test2", AF_UNIX, SOCK_STREAM);
        REQUIRE_FALSE(addr1 == addr2);
        REQUIRE(addr1 != addr2);
        REQUIRE(addr1 < addr2);
        REQUIRE_FALSE(addr1 > addr2);
        REQUIRE(addr1 <= addr2);
        REQUIRE_FALSE(addr1 >= addr2);
    }
}

TEST_CASE("sock_addr hash function", "[sock_addr]") {
    SECTION("equal addresses have equal hashes") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) == std::hash<sock_addr>{}(addr2));
    }

    SECTION("different ipv4 addresses have different hashes") {
        sock_addr addr1("127.0.0.1:8080", AF_INET, SOCK_STREAM);
        sock_addr addr2("127.0.0.2:8080", AF_INET, SOCK_STREAM);
        sock_addr addr3("127.0.0.1:8081", AF_INET, SOCK_STREAM);
        
        auto hash1 = std::hash<sock_addr>{}(addr1);
        auto hash2 = std::hash<sock_addr>{}(addr2);
        auto hash3 = std::hash<sock_addr>{}(addr3);
        
        REQUIRE(hash1 != hash2);
        REQUIRE(hash1 != hash3);
        REQUIRE(hash2 != hash3);
    }

    SECTION("equal ipv6 addresses have equal hashes") {
        sock_addr addr1("[2001:db8::1]:8080", AF_INET6, SOCK_STREAM);
        sock_addr addr2("[2001:db8::1]:8080", AF_INET6, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) == std::hash<sock_addr>{}(addr2));
    }

    SECTION("different ipv6 addresses have different hashes") {
        sock_addr addr1("[2001:db8::1]:8080", AF_INET6, SOCK_STREAM);
        sock_addr addr2("[2001:db8::2]:8080", AF_INET6, SOCK_STREAM);
        sock_addr addr3("[2001:db8::1]:8081", AF_INET6, SOCK_STREAM);
        
        auto hash1 = std::hash<sock_addr>{}(addr1);
        auto hash2 = std::hash<sock_addr>{}(addr2);
        auto hash3 = std::hash<sock_addr>{}(addr3);
        
        REQUIRE(hash1 != hash2);
        REQUIRE(hash1 != hash3);
        REQUIRE(hash2 != hash3);
    }

    SECTION("equal unix domain sockets have equal hashes") {
        sock_addr addr1("/tmp/test.sock", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("/tmp/test.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) == std::hash<sock_addr>{}(addr2));
    }

    SECTION("different unix domain sockets have different hashes") {
        sock_addr addr1("/tmp/test1.sock", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("/tmp/test2.sock", AF_UNIX, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) != std::hash<sock_addr>{}(addr2));
    }

    SECTION("equal abstract unix sockets have equal hashes") {
        sock_addr addr1("@test", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("@test", AF_UNIX, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) == std::hash<sock_addr>{}(addr2));
    }

    SECTION("different abstract unix sockets have different hashes") {
        sock_addr addr1("@test1", AF_UNIX, SOCK_STREAM);
        sock_addr addr2("@test2", AF_UNIX, SOCK_STREAM);
        REQUIRE(std::hash<sock_addr>{}(addr1) != std::hash<sock_addr>{}(addr2));
    }

    SECTION("verify unordered_map usage") {
        std::unordered_map<sock_addr, std::string> addr_map;
        
        sock_addr addr1("127.0.0.1:80", AF_INET);
        sock_addr addr2("[::1]:80", AF_INET6);
        sock_addr addr3("/tmp/test.sock", AF_UNIX);
        sock_addr addr4("@abstract", AF_UNIX);
        
        addr_map[addr1] = "ipv4";
        addr_map[addr2] = "ipv6";
        addr_map[addr3] = "unix";
        addr_map[addr4] = "abstract";
        
        REQUIRE(addr_map.size() == 4);
        REQUIRE(addr_map[addr1] == "ipv4");
        REQUIRE(addr_map[addr2] == "ipv6");
        REQUIRE(addr_map[addr3] == "unix");
        REQUIRE(addr_map[addr4] == "abstract");
    }
}

TEST_CASE("network_range basic operations", "[network_range]") {
    SECTION("default constructor") {
        network_range net;
        REQUIRE_FALSE(net.valid());
        REQUIRE(net.prefix() == 0);
    }

    SECTION("invalid address family") {
        sock_addr unix_addr("/tmp/test.sock", AF_UNIX);
        network_range net(unix_addr);
        REQUIRE_FALSE(net.valid());
    }
}

// Update remaining test cases to use network_range instead of sock_mask
// while keeping test logic identical

TEST_CASE("network_range IPv4", "[network_range]") {
    SECTION("class C network") {
        sock_addr net("192.168.1.0/24", AF_INET);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 24);
        REQUIRE(mask.network().to_string() == "192.168.1.0");

        sock_addr addr1("192.168.1.1", AF_INET);
        sock_addr addr2("192.168.1.254", AF_INET);
        sock_addr addr3("192.168.2.1", AF_INET);

        REQUIRE(mask.contains(addr1));
        REQUIRE(mask.contains(addr2));
        REQUIRE_FALSE(mask.contains(addr3));
    }

    SECTION("single host") {
        sock_addr net("10.0.0.1/32", AF_INET);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 32);

        sock_addr addr1("10.0.0.1", AF_INET);
        sock_addr addr2("10.0.0.2", AF_INET);

        REQUIRE(mask.contains(addr1));
        REQUIRE_FALSE(mask.contains(addr2));
    }

    SECTION("class A network") {
        sock_addr net("10.0.0.0/8", AF_INET);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 8);

        sock_addr addr1("10.1.2.3", AF_INET);
        sock_addr addr2("10.255.255.255", AF_INET);
        sock_addr addr3("11.0.0.0", AF_INET);

        REQUIRE(mask.contains(addr1));
        REQUIRE(mask.contains(addr2));
        REQUIRE_FALSE(mask.contains(addr3));
    }
}

TEST_CASE("network_range IPv6", "[network_range]") {
    SECTION("typical subnet") {
        sock_addr net("[2001:db8::]/64", AF_INET6);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 64);
        REQUIRE(mask.network().to_string() == "[2001:db8::]");

        sock_addr addr1("[2001:db8::1]", AF_INET6);
        sock_addr addr2("[2001:db8::ffff]", AF_INET6);
        sock_addr addr3("[2001:db8:1::]", AF_INET6);

        REQUIRE(mask.contains(addr1));
        REQUIRE(mask.contains(addr2));
        REQUIRE_FALSE(mask.contains(addr3));
    }

    SECTION("single host") {
        sock_addr net("[2001:db8::1]/128", AF_INET6);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 128);

        sock_addr addr1("[2001:db8::1]", AF_INET6);
        sock_addr addr2("[2001:db8::2]", AF_INET6);

        REQUIRE(mask.contains(addr1));
        REQUIRE_FALSE(mask.contains(addr2));
    }

    SECTION("site local prefix") {
        sock_addr net("[fc00::]/7", AF_INET6);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 7);

        sock_addr addr1("[fc00::1]", AF_INET6);
        sock_addr addr2("[fd00::1]", AF_INET6);
        sock_addr addr3("[fe00::1]", AF_INET6);

        REQUIRE(mask.contains(addr1));
        REQUIRE(mask.contains(addr2));
        REQUIRE_FALSE(mask.contains(addr3));
    }
}

TEST_CASE("network_range edge cases", "[network_range]") {
    SECTION("zero prefix") {
        sock_addr net("0.0.0.0/0", AF_INET);
        network_range mask(net);
        REQUIRE(mask.valid());
        REQUIRE(mask.prefix() == 0);

        sock_addr addr1("192.168.1.1", AF_INET);
        sock_addr addr2("10.0.0.1", AF_INET);
        
        REQUIRE(mask.contains(addr1));
        REQUIRE(mask.contains(addr2));
    }

    SECTION("different address families") {
        sock_addr net("192.168.1.0/24", AF_INET);
        network_range mask(net);
        REQUIRE(mask.valid());

        sock_addr addr("[2001:db8::1]", AF_INET6);
        REQUIRE_FALSE(mask.contains(addr));
    }

    SECTION("invalid prefix handling") {
        sock_addr net1("192.168.1.0/33", AF_INET);
        network_range mask1(net1);
        REQUIRE(!mask1.valid());
        REQUIRE(mask1.prefix() == 0);  // Prefix should be 0 for invalid mask

        sock_addr net2("[2001:db8::]/129", AF_INET6);
        network_range mask2(net2);
        REQUIRE(!mask2.valid());
        REQUIRE(mask2.prefix() == 0);  // Prefix should be 0 for invalid mask
        
        // Invalid masks should not match any address
        sock_addr addr1("192.168.1.1", AF_INET);
        sock_addr addr2("192.168.1.2", AF_INET);
        REQUIRE_FALSE(mask1.contains(addr1));
        REQUIRE_FALSE(mask1.contains(addr2));
        
        sock_addr addr3("[2001:db8::1]", AF_INET6);
        sock_addr addr4("[2001:db8::2]", AF_INET6);
        REQUIRE_FALSE(mask2.contains(addr3));
        REQUIRE_FALSE(mask2.contains(addr4));
    }
}

TEST_CASE("network_range netmask parsing", "[network_range]") {
    SECTION("ipv4 with netmask string") {
        network_range net("192.168.1.0/255.255.255.0", AF_INET);
        REQUIRE(net.valid());
        REQUIRE(net.prefix() == 24);
        REQUIRE(net.network().to_string() == "192.168.1.0");

        sock_addr addr1("192.168.1.1", AF_INET);
        sock_addr addr2("192.168.1.255", AF_INET);
        sock_addr addr3("192.168.2.1", AF_INET);

        REQUIRE(net.contains(addr1));
        REQUIRE(net.contains(addr2));
        REQUIRE_FALSE(net.contains(addr3));
    }

    SECTION("invalid netmask string") {
        network_range net("192.168.1.0/255.255.256.0", AF_INET);
        REQUIRE_FALSE(net.valid());
    }

    SECTION("non-contiguous netmask") {
        network_range net("192.168.1.0/255.0.255.0", AF_INET);
        REQUIRE_FALSE(net.valid());
    }
}

TEST_CASE("network_range mixed address families", "[network_range]") {
    SECTION("ipv4 network with ipv6 address") {
        network_range net("192.168.1.0/24", AF_INET);
        sock_addr addr("[::1]", AF_INET6);
        REQUIRE_FALSE(net.contains(addr));
    }

    SECTION("ipv6 network with ipv4 address") {
        network_range net("[2001:db8::]/64", AF_INET6);
        sock_addr addr("192.168.1.1", AF_INET);
        REQUIRE_FALSE(net.contains(addr));
    }
}

TEST_CASE("network_range construction variants", "[network_range]") {
    SECTION("explicit address family override") {
        network_range net1("192.168.1.0/24", AF_INET);
        REQUIRE(net1.valid());
        
        network_range net2("192.168.1.0/24", AF_INET6);  // Should fail due to mismatch
        REQUIRE_FALSE(net2.valid());
    }

    SECTION("zero length prefix") {
        network_range ipv4_all("0.0.0.0/0", AF_INET);
        REQUIRE(ipv4_all.valid());
        REQUIRE(ipv4_all.prefix() == 0);
        
        network_range ipv6_all("[::]/0", AF_INET6);
        REQUIRE(ipv6_all.valid());
        REQUIRE(ipv6_all.prefix() == 0);
    }
}

TEST_CASE("sock_addr with loopback addresses", "[sock_addr]") {
    SECTION("ipv4 loopback variants") {
        sock_addr addr1("127.0.0.1", AF_INET);
        sock_addr addr2("localhost", AF_INET);
        REQUIRE(addr1.family() == AF_INET);
        REQUIRE(addr2.family() == AF_INET);
        // Note: addr1 == addr2 may not always be true due to DNS resolution
    }

    SECTION("ipv6 loopback variants") {
        sock_addr addr1("[::1]", AF_INET6);
        sock_addr addr2("[localhost]", AF_INET6);
        REQUIRE(addr1.family() == AF_INET6);
        REQUIRE(addr2.family() == AF_INET6);
    }
}

