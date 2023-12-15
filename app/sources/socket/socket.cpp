#include "socket.h"

#include <cstring>

#include "../logger/logger.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

Socket::Socket(int sockNum) noexcept :
    IODevice(),
    sock_(sockNum),
    maxConnections_ { 0 },
    asycnSocket_ { false }
{
}

Socket::Socket(const std::string &address, int portNum, SocketType st) noexcept :
    IODevice(),
    socketAddress_ { address },
    socketPortNum_ { portNum },
    sockType_ { st }
{
    sock_   = socket(toNixSocketType(), SOCK_STREAM, IPPROTO_TCP);
    int val = 1;

    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) < 0)
    {
        LOG_ERROR("set SO_REUSEADDR failed");
    }
}

Socket::~Socket()
{
    // For AF_UNIX you can use call unlink (path); after close() socket in "server" app

    if (sock_ > 0)
    {
        auto res = ::shutdown(sock_, 2);

        if (res < 0)
        {
            handleError("Can't close socket");
        }
        else
        {
            sock_ = -1;
        }
    }
}

bool Socket::open(OpenMode mode)
{
    if (sock_ < 0)
    {
        LOG_CRITICAL("Error, can't open socket");
        return false;
    }

    if (sockType_ == SocketType::ETHERNET)
    {
        if (!makeBindInet())
        {
            return false;
        }
    }
    else
    {
        if (!makeBindLocal())
        {
            return false;
        }
    }

    if (!listen())
    {
        return false;
    }

    return true;
}

bool Socket::close()
{
    if (sock_ > 0)
    {
        auto res = ::shutdown(sock_, 2);
        if (res < 0)
        {
            handleError("Can't close socket:");
            return false;
        }
        else
        {
            sock_ = -1;
            return true;
        }
    }
    return true;
}

bool Socket::isOpened()
{
    return true;
}

bool Socket::listen()
{
    auto res = ::listen(sock_, maxConnections_);
    if (res < 0)
    {
        handleError("Error, can't listen socket:");
        return false;
    }

    return true;
}

std::shared_ptr< Socket > Socket::accept()
{
    struct sockaddr_in cli_addr;
    socklen_t          addr_size = sizeof(cli_addr);

    int newsockfd = ::accept(sock_, ( struct sockaddr * )&cli_addr, &addr_size);

    if (newsockfd < 0)
    {
        handleError("Can't accept connection:");
        return nullptr;
    }

    LOG_INFO("Get connection to server from:", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    return std::make_shared< Socket >(newsockfd);
}

bool Socket::connect()
{
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(socketAddress_.c_str());
    serv_addr.sin_port        = htons(socketPortNum_);

    auto res = ::connect(sock_, ( struct sockaddr * )&serv_addr, sizeof(serv_addr));

    if (errno == EINPROGRESS)
    {
        return false;
    }
    if (res < 0)
    {
        LOG_ERROR("Error on connection to: ", socketAddress_, " : ", socketPortNum_);
        handleError("Error:");
        return false;
    }
    return true;
}

int Socket::getFd() const
{
    return sock_;
}

void Socket::setMaximumConnectionsHandle(int maxConnections)
{
    maxConnections_ = maxConnections;
}

bool Socket::nonBlockingMode()
{
    if (fcntl(sock_, F_SETFL, fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        LOG_ERROR("Can't set nonblocking mode to socket");
        return false;
    }

    return true;
}

int Socket::bytesAviable()
{
    int  number_of_bytes_available = 0;
    bool is_data_available         = false;

    const auto ioctl_result = ioctl(sock_, FIONREAD, &number_of_bytes_available);

    if ((ioctl_result >= 0) && (number_of_bytes_available > 0))
    {
        return -1;
    }

    return number_of_bytes_available;
}

int Socket::read(std::vector< uint8_t > &data, int size)
{
    // auto res = ::read(sock_, data.data(), (size == -1 ? data.size() : size));
    auto res = ::recv(sock_, data.data(), (size == -1 ? data.size() : size), 0);
    return res;
}

int Socket::write(std::vector< uint8_t > &data, int size)
{
    auto res = ::write(sock_, data.data(), (size == -1 ? data.size() : size));
    if (res < 0) handleError("Can't write:");
    return res;
}

int Socket::write(const DatatPackage &pkg)
{
    std::vector< uint8_t > buffer;
    auto                   size        = pkg.generatePackage(buffer);
    auto                   writeResult = ::write(sock_, buffer.data(), size);

    if (writeResult < 0) handleError("Can't write:");
    return writeResult;
}

int Socket::toNixSocketType() const noexcept
{
    switch (sockType_)
    {
    case SocketType::LOCAL:
        return AF_LOCAL;
    case SocketType::ETHERNET:
        return AF_INET;
    default:
        return -1;
    }
}

bool Socket::makeBindLocal() noexcept
{
    struct sockaddr_un name;
    name.sun_family = toNixSocketType();
    strncpy(name.sun_path, socketAddress_.c_str(), sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    auto size = (offsetof(struct sockaddr_un, sun_path) + strlen(name.sun_path));

    if (bind(sock_, ( struct sockaddr * )&name, size) < 0)
    {
        LOG_ERROR("Error on binding local socket: ", socketAddress_);
        return false;
    }

    LOG_INFO("Socket binded at:", socketAddress_);
    return true;
}

bool Socket::makeBindInet() noexcept
{
    struct sockaddr_in name;

    name.sin_family      = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    name.sin_port        = htons(socketPortNum_);

    if (bind(sock_, ( struct sockaddr * )&name, sizeof(name)) < 0)
    {
        handleError("Error on binding inet socket:");
        return false;
    }

    return true;
}

void Socket::handleError(const std::string &someMessage)
{
    LOG_ERROR(someMessage, std::strerror(errno));
}
