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

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace
{
    sockaddr_in make_ipv4_addr(std::string_view ip, uint16_t port)
    {
        sockaddr_in ipAddress{};
        ipAddress.sin_family = AF_INET;
        ipAddress.sin_port = htons(port);

        int rc = ::inet_pton(AF_INET, std::string(ip).c_str(), &ipAddress.sin_addr);
        if (rc == 0)
            throw std::invalid_argument("make_ipv4_addr: Invalid IPv4 address");
        if (rc < 0)
            throw std::system_error(errno, std::generic_category(), "inet_pton");
        return ipAddress;
    }
}
namespace proxy
{

    Socket::~Socket()
    {
        if (_fd >= 0)
            close(_fd);
    }
    Socket &Socket::operator=(Socket &&other) noexcept
    {
        if (this == &other)
            return *this;

        if (_fd >= 0)
            Socket toClose(_fd);

        this->_fd = std::exchange(other._fd, -1);
        return *this;
    }
    Socket Socket::create_tcp()
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        int enable = 1;

        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), std::string("socket"));

        Socket newSoc(fd);

        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            throw std::system_error(errno, std::generic_category(), std::string("setsockopt"));

        return newSoc;
    }
    void Socket::connect_to(std::string_view ip, uint16_t port)
    {
        const sockaddr_in serverIP = make_ipv4_addr(ip, port);
        if (::connect(_fd, reinterpret_cast<const sockaddr *>(&serverIP), sizeof(serverIP)))
            throw std::system_error(errno, std::generic_category(), std::string("connect"));
    }
    Socket Socket::accept_client() const
    {
        int clientFd = ::accept(_fd, nullptr, nullptr);
        if (clientFd < 0)
            throw std::system_error(errno, std::generic_category(), std::string("accept"));
        return Socket(clientFd);
    }
    void Socket::bind_and_listen(std::string_view ip, uint16_t port, int backlog)
    {

        const sockaddr_in serverAddress = make_ipv4_addr(ip, port);
        if (::bind(_fd, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)))
            throw std::system_error(errno, std::generic_category(), std::string("bind"));
        if (::listen(_fd, backlog))
            throw std::system_error(errno, std::generic_category(), std::string("listen"));
    }
    void Socket::send_all(std::span<const std::byte> data) const
    {
        ssize_t bytesSent = 0;
        for (auto it = data.begin(); it != data.end();)
        {
            bytesSent = ::send(_fd, &*it, static_cast<std::size_t>(data.end() - it), MSG_NOSIGNAL);
            if (bytesSent < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    throw std::system_error(errno, std::generic_category(), std::string("send"));
                }
            }
            if (bytesSent == 0)
                throw std::runtime_error("send returned 0");
            it += bytesSent;
        }
    }
    std::ptrdiff_t Socket::recv_some(std::span<std::byte> buffer) const
    {
        ssize_t bytesRecieved = 0;
        while (true)
        {
            bytesRecieved = ::recv(_fd, buffer.data(), buffer.size(), 0);
            if (bytesRecieved < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), std::string("recv"));
            }
            break;
        }

        return bytesRecieved;
    }

    void Socket::shutdown_write() const
    {
        int shutdownStatus = ::shutdown(_fd, SHUT_WR) < 0;
        if (shutdownStatus == ENOTCONN)
            return;
        throw std::system_error(errno, std::generic_category(), std::string("recv"));
    }

}
