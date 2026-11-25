#include "proxy/socket.hpp"
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstddef>

namespace
{
    sockaddr_in make_ipv4_addr(std::string_view ip, uint16_t port)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        const std::string ip_str(ip);
        const int rc = ::inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr);
        if (rc == 0)
        {
            throw std::invalid_argument("Invalid IPv4 address format: " + ip_str);
        }
        if (rc < 0)
        {
            throw std::system_error(errno, std::generic_category(), "inet_pton() failed");
        }
        return addr;
    }
}
namespace proxy
{

    Socket::~Socket()
    {
        if (fd_ >= 0)
        {
            close(fd_);
        }
    }
    Socket &Socket::operator=(Socket &&other) noexcept
    {
        if (this != &other)
        {
            if (fd_ >= 0)
            {
                close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    Socket Socket::create_tcp()
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            throw std::system_error(errno, std::generic_category(), "socket() failed");
        }

        Socket sock(fd);
        const int enable = 1;
        if (::setsockopt(sock.native_handle(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        {
            throw std::system_error(errno, std::generic_category(), "setsockopt(SO_REUSEADDR) failed");
        }
        return sock;
    }
    void Socket::connect_to(std::string_view ip, uint16_t port)
    {
        sockaddr_in addr = make_ipv4_addr(ip, port);
        if (connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            throw std::system_error(errno, std::generic_category(), "connect() failed");
        }
    }
    Socket Socket::accept_client() const
    {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
        if (client_fd < 0)
        {
            throw std::system_error(errno, std::generic_category(), "accept() failed");
        }
        return Socket(client_fd);
    }
    void Socket::bind_and_listen(std::string_view ip, uint16_t port, int backlog)
    {
        sockaddr_in addr = make_ipv4_addr(ip, port);
        if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            throw std::system_error(errno, std::generic_category(), "bind() failed");
        }
        if (::listen(fd_, backlog) < 0)
        {
            throw std::system_error(errno, std::generic_category(), "listen() failed");
        }
    }
    void Socket::send_all(std::span<const std::byte> data) const
    {
        std::size_t total_sent = 0;
        while (total_sent < data.size())
        {
            const ssize_t bytes_sent = ::send(
                fd_,
                data.data() + total_sent,
                data.size() - total_sent,
                MSG_NOSIGNAL);
            if (bytes_sent < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "send() failed");
            }
            if (bytes_sent == 0)
            {
                throw std::runtime_error("send() returned 0 bytes");
            }
            total_sent += static_cast<std::size_t>(bytes_sent);
        }
    }
    std::ptrdiff_t Socket::recv_some(std::span<std::byte> buffer) const
    {
        while (true)
        {
            const ssize_t bytes_read = ::recv(fd_, buffer.data(), buffer.size(), 0);
            if (bytes_read < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "recv() failed");
            }
            return bytes_read; // 0 means peer closed connection (EOF)
        }
    }
}
