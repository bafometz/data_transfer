#include "socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <format>
#include <netinet/in.h>
#include <span>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "../helpers/error.h"
#include "../logger/logger.h"

Socket::Socket(int sockNum) noexcept
    : IODevice()
    , asycnSocket_ { false }
    , maxConnections_ { 0 }
    , sock_(sockNum)
{
}

Socket::Socket(const std::string &address, int portNum, SocketType st, bool nonBlockingMode)
    : IODevice()
    , socketPortNum_ { portNum }
    , sockType_ { st }
    , socketAddress_ { address }
{
    int sockopt = SOCK_STREAM;

    if (nonBlockingMode) {
        sockopt |= SOCK_NONBLOCK;
        asycnSocket_ = true;
    }

    sock_ = socket(toNixSocketType(), sockopt, IPPROTO_TCP);
    if(sock_ == -1)
        throw data_transfer::errors::exception("Can't create socket: {}", data_transfer::errors::system_error{});

    int val = 1;
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) < 0) {
        OLD_LOG_ERROR("set SO_REUSEADDR failed");
    }
}

Socket::~Socket()
{
    close();

    if (SocketType::LOCAL == sockType_) { // For AF_UNIX | AF_LOCAL you can use call unlink (path); after close() socket in "server" app
        ::unlink(socketAddress_.c_str());
    }
}

bool Socket::open([[maybe_unused]] OpenMode mode)
{
    if (sock_ < 0) {
        OLD_LOG_CRITICAL("Error, can't open socket");
        return false;
    }

    if (sockType_ == SocketType::ETHERNET) {
        if (!makeBindInet()) {
            return false;
        }
    } else {
        if (!makeBindLocal()) {
            return false;
        }
    }

    if (!listen()) {
        return false;
    }

    return true;
}

bool Socket::close()
{
    if (sock_ < 0)
        return true;

    const auto res = ::close(sock_);
    if (res != 0) {
        LOG_CRITICAL("Can't close socket({}): {}", res, data_transfer::errors::system_error {});
        return false;
    }

    sock_ = -1;
    return true;
}

bool Socket::isOpened()
{
    return sock_ != -1;
}

bool Socket::listen()
{
    const auto result = (::listen(sock_, maxConnections_) == 0);
    if (!result)
        LOG_ERROR("listen(fd, int) finished with error: {}", data_transfer::errors::system_error {});

    return result;
}

std::shared_ptr<Socket> Socket::accept()
{
    struct sockaddr_in cli_addr;
    socklen_t addr_size = sizeof(cli_addr);
    const auto newsockfd = ::accept(sock_, (struct sockaddr *) &cli_addr, &addr_size);

    if (newsockfd < 0) {
        LOG_ERROR("Can't accept connection: {}", data_transfer::errors::system_error {});
        return nullptr;
    }

    LOG_INFO("Accepted connection from: {}:{}", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    return std::make_shared<Socket>(newsockfd);
}

bool Socket::connect()
{
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(socketAddress_.c_str());
    serv_addr.sin_port = htons(socketPortNum_);

    auto res = ::connect(sock_, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    if (res < 0) {
        if (errno == EINPROGRESS)
            return false;
        LOG_ERROR("Connection error to: {}:{}", socketAddress_, socketPortNum_);
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

bool Socket::shutdown() noexcept{
    const auto success = (::shutdown(sock_, SHUT_RDWR) == 0);
    if(!success) {
        LOG_ERROR("Can't shotdown socket: {}", data_transfer::errors::system_error{});
        return false;
    }

    return true;
}

bool Socket::setNonBlockMode()
{
    if (fcntl(sock_, F_SETFL, fcntl(sock_, F_GETFL, 0) | O_NONBLOCK) == -1) {
        LOG_ERROR("Can't set nonblocking mode to socket {}", data_transfer::errors::system_error {});
        return false;
    }

    return true;
}

int Socket::bytesAviable()
{
    int number_of_bytes_available = 0;
    const auto ioctl_result = ioctl(sock_, FIONREAD, &number_of_bytes_available);

    if (ioctl_result == -1) {
        LOG_ERROR("ioctl finished with error, can't get aviabled bytes {}", data_transfer::errors::system_error {});
        return -1;
    }

    return number_of_bytes_available;
}

ssize_t Socket::read(std::span<std::byte> container)
{
    for (;;) {
        const auto bytes_readed = ::recv(sock_, container.data(), container.size(), 0);
        if (bytes_readed >= 0)
            return bytes_readed;

        if (errno == EINTR)
            continue;

        // Неблокирующий режим: данных сейчас нет. Возвращаем -1, вызывающая сторона
        // должна отличать EAGAIN от реальной ошибки/разрыва по errno.
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -1;

        LOG_ERROR("recv() failed: {}", data_transfer::errors::system_error {});
        return -1;
    }
}

ssize_t Socket::write(std::span<const std::byte> buffer)
{
    for (;;) {
        const auto bytes_written = ::write(sock_, buffer.data(), buffer.size());
        if (bytes_written >= 0)
            return bytes_written;

        if (errno == EINTR)
            continue;

        // Неблокирующий режим: буфер отправки переполнен. Возвращаем -1,
        // вызывающая сторона должна отличать EAGAIN по errno.
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -1;

        LOG_ERROR("write() failed: {}", data_transfer::errors::system_error {});
        return -1;
    }
}

ssize_t Socket::write(const DatatPackage &pkg)
{
    std::vector<uint8_t> buffer;
    const auto size = pkg.generatePackage(buffer);
    const auto writeResult = ::write(sock_, buffer.data(), size);

    if (writeResult < 0)
        LOG_ERROR("Can't write data to socket: {}", data_transfer::errors::system_error {});

    return writeResult;
}

int Socket::toNixSocketType() const noexcept
{
    switch (sockType_) {
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

    const auto size = (offsetof(struct sockaddr_un, sun_path) + strlen(name.sun_path));

    if (bind(sock_, (struct sockaddr *) &name, size) < 0) {
        LOG_ERROR("Can't bind socket: {}", data_transfer::errors::system_error {});
        return false;
    }

    LOG_INFO("Socket bindet at: {}", socketAddress_);
    return true;
}

bool Socket::makeBindInet() noexcept
{
    struct sockaddr_in name;

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    name.sin_port = htons(socketPortNum_);
    if (bind(sock_, (struct sockaddr *) &name, sizeof(name)) < 0) {
        LOG_ERROR("Can't bind socket: {}", data_transfer::errors::system_error {});
        return false;
    }

    return true;
}
