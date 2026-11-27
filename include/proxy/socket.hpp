#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace proxy {

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept : _fd(fd) {}
    ~Socket();

    // Prevent double-close by forbidding copy
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Transfer ownership safely
    Socket(Socket&& other) noexcept : _fd(std::exchange(other._fd, -1)) {}
    Socket& operator=(Socket&& other) noexcept;

    //allocate a TCP socket and set SO_REUSEADDR
    [[nodiscard]] static Socket create_tcp();
    //set socket to listen on the given IP and port
    void bind_and_listen(std::string_view ip, uint16_t port, int backlog = 128);
    //accept a client connection and return a new Socket for it
    [[nodiscard]] Socket accept_client() const;
    //connect to a remote server at the given IP and port
    void connect_to(std::string_view ip, uint16_t port);

    [[nodiscard]] std::ptrdiff_t recv_some(std::span<std::byte> buffer) const;
    void send_all(std::span<const std::byte> data) const;
    void shutdown_write() const;

    [[nodiscard]] int native_handle() const noexcept { return _fd; }
    [[nodiscard]] bool is_valid() const noexcept { return _fd >= 0; }

private:
    int _fd{-1};
};

} // namespace proxy