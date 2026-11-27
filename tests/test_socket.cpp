#include "proxy/socket.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <vector>

TEST(SocketTest, CreateTcpSocket) {
    auto sock = proxy::Socket::create_tcp();
    EXPECT_TRUE(sock.is_valid());
    EXPECT_GE(sock.native_handle(), 0);
}

TEST(SocketTest, MoveSemantics) {
    auto sock1 = proxy::Socket::create_tcp();
    const int fd1 = sock1.native_handle();

    proxy::Socket sock2(std::move(sock1));
    EXPECT_FALSE(sock1.is_valid());
    EXPECT_EQ(sock1.native_handle(), -1);
    EXPECT_TRUE(sock2.is_valid());
    EXPECT_EQ(sock2.native_handle(), fd1);

    proxy::Socket sock3;
    sock3 = std::move(sock2);
    EXPECT_FALSE(sock2.is_valid());
    EXPECT_EQ(sock2.native_handle(), -1);
    EXPECT_TRUE(sock3.is_valid());
    EXPECT_EQ(sock3.native_handle(), fd1);
}

TEST(SocketTest, InvalidIpAddressThrows) {
    auto sock = proxy::Socket::create_tcp();
    EXPECT_THROW(sock.bind_and_listen("invalid_ip", 8080), std::invalid_argument);
    EXPECT_THROW(sock.connect_to("999.999.999.999", 8080), std::invalid_argument);
}

TEST(SocketTest, ConnectFailureThrows) {
    uint16_t closed_port = 0;
    {
        auto temp_server = proxy::Socket::create_tcp();
        temp_server.bind_and_listen("127.0.0.1", 0);
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        ASSERT_EQ(::getsockname(temp_server.native_handle(), reinterpret_cast<sockaddr*>(&addr), &len), 0);
        closed_port = ntohs(addr.sin_port);
    }

    auto sock = proxy::Socket::create_tcp();
    EXPECT_THROW(sock.connect_to("127.0.0.1", closed_port), std::system_error);
}

TEST(SocketTest, LoopbackTcpTransfer) {
    auto server = proxy::Socket::create_tcp();
    server.bind_and_listen("127.0.0.1", 0);

    sockaddr_in sin{};
    socklen_t len = sizeof(sin);
    ASSERT_EQ(::getsockname(server.native_handle(), reinterpret_cast<sockaddr*>(&sin), &len), 0);
    const uint16_t port = ntohs(sin.sin_port);

    // 8KB payload
    std::vector<std::byte> payload(8192, std::byte{0x42});

    std::jthread client_thread([port, &payload]() {
        auto client = proxy::Socket::create_tcp();
        client.connect_to("127.0.0.1", port);
        client.send_all(payload);
    });

    auto peer = server.accept_client();
    ASSERT_TRUE(peer.is_valid());

    std::vector<std::byte> received;
    received.reserve(payload.size());
    std::array<std::byte, 1024> chunk{};

    while (received.size() < payload.size()) {
        const std::ptrdiff_t bytes_read = peer.recv_some(chunk);
        ASSERT_GT(bytes_read, 0);
        received.insert(received.end(), chunk.begin(), chunk.begin() + bytes_read);
    }

    EXPECT_EQ(received.size(), payload.size());
    EXPECT_EQ(received, payload);
}
