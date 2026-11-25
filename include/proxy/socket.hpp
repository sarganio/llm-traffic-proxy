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
    explicit Socket(int fd) noexcept : fd_(fd) {}
    ~Socket();

    // Prevent double-close by forbidding copy
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Transfer ownership safely
    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] static Socket create_tcp();

    void bind_and_listen(std::string_view ip, uint16_t port, int backlog = 128);
    [[nodiscard]] Socket accept_client() const;
    void connect_to(std::string_view ip, uint16_t port);

    [[nodiscard]] std::ptrdiff_t recv_some(std::span<std::byte> buffer) const;
    void send_all(std::span<const std::byte> data) const;

    [[nodiscard]] int native_handle() const noexcept { return fd_; }
    [[nodiscard]] bool is_valid() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};

} // namespace proxy