#include <gtest/gtest.h>
#include "proxy/socket.hpp"
#include "proxy/tunnel.hpp"
#include <thread>
#include <vector>
#include <array>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>

namespace {
    uint16_t get_bound_port(const proxy::Socket& sock) {
        sockaddr_in sin{};
        socklen_t len = sizeof(sin);
        if (::getsockname(sock.native_handle(), reinterpret_cast<sockaddr*>(&sin), &len) != 0) {
            throw std::runtime_error("getsockname failed");
        }
        return ntohs(sin.sin_port);
    }
}

TEST(TunnelTest, BidirectionalEchoAndHalfClose) {
    auto upstream_server = proxy::Socket::create_tcp();
    upstream_server.bind_and_listen("127.0.0.1", 0);
    const uint16_t upstream_port = get_bound_port(upstream_server);

    std::vector<std::byte> req_payload(32768, std::byte{0xAA});
    std::vector<std::byte> res_payload(32768, std::byte{0xBB});

    // Mock upstream server
    std::jthread upstream_thread([&upstream_server, &res_payload]() {
        auto conn = upstream_server.accept_client();
        std::array<std::byte, 4096> buf{};
        while (conn.recv_some(buf) > 0) {
            // Read until client EOF
        }
        conn.send_all(res_payload);
    });

    auto proxy_server = proxy::Socket::create_tcp();
    proxy_server.bind_and_listen("127.0.0.1", 0);
    const uint16_t proxy_port = get_bound_port(proxy_server);

    // Proxy relay thread
    std::jthread proxy_thread([&proxy_server, upstream_port]() {
        auto client_conn = proxy_server.accept_client();
        auto upstream_conn = proxy::Socket::create_tcp();
        upstream_conn.connect_to("127.0.0.1", upstream_port);
        proxy::relay_bidirectional(client_conn, upstream_conn);
    });

    // Client
    auto client = proxy::Socket::create_tcp();
    client.connect_to("127.0.0.1", proxy_port);
    
    // Send request and half-close
    client.send_all(req_payload);
    client.shutdown_write();

    // Receive response
    std::vector<std::byte> received;
    std::array<std::byte, 4096> buf{};
    while (true) {
        const auto bytes_read = client.recv_some(buf);
        if (bytes_read == 0) break;
        received.insert(received.end(), buf.begin(), buf.begin() + bytes_read);
    }

    EXPECT_EQ(received.size(), res_payload.size());
    EXPECT_EQ(received, res_payload);
}

TEST(TunnelTest, ConcurrentFullDuplex) {
    auto upstream_server = proxy::Socket::create_tcp();
    upstream_server.bind_and_listen("127.0.0.1", 0);
    const uint16_t upstream_port = get_bound_port(upstream_server);

    std::vector<std::byte> up_payload(16384, std::byte{0xCC});
    std::vector<std::byte> down_payload(16384, std::byte{0xDD});

    // Mock upstream server
    std::jthread upstream_thread([&upstream_server, &up_payload, &down_payload]() {
        auto conn = upstream_server.accept_client();
        
        // Upstream sends data concurrently
        std::jthread sender([&conn, &down_payload]() {
            conn.send_all(down_payload);
            conn.shutdown_write();
        });

        // Upstream receives data
        std::vector<std::byte> received;
        std::array<std::byte, 4096> buf{};
        while (true) {
            const auto bytes_read = conn.recv_some(buf);
            if (bytes_read == 0) break;
            received.insert(received.end(), buf.begin(), buf.begin() + bytes_read);
        }
        EXPECT_EQ(received, up_payload);
    });

    auto proxy_server = proxy::Socket::create_tcp();
    proxy_server.bind_and_listen("127.0.0.1", 0);
    const uint16_t proxy_port = get_bound_port(proxy_server);

    // Proxy relay thread
    std::jthread proxy_thread([&proxy_server, upstream_port]() {
        auto client_conn = proxy_server.accept_client();
        auto upstream_conn = proxy::Socket::create_tcp();
        upstream_conn.connect_to("127.0.0.1", upstream_port);
        proxy::relay_bidirectional(client_conn, upstream_conn);
    });

    // Client
    auto client = proxy::Socket::create_tcp();
    client.connect_to("127.0.0.1", proxy_port);
    
    // Client sends data concurrently
    std::jthread client_sender([&client, &up_payload]() {
        client.send_all(up_payload);
        client.shutdown_write();
    });

    // Client receives data
    std::vector<std::byte> received;
    std::array<std::byte, 4096> buf{};
    while (true) {
        const auto bytes_read = client.recv_some(buf);
        if (bytes_read == 0) break;
        received.insert(received.end(), buf.begin(), buf.begin() + bytes_read);
    }

    EXPECT_EQ(received, down_payload);
}
