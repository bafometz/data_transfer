#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <vector>

#include "../data_package/datatpackage.h"
#include "../io_device/iodevice.h"
#include <atomic>
#include <memory>

class Socket : public IODevice
{
    enum class SocketType
    {
        LOCAL,
        ETHERNET,
    };

  public:
    explicit Socket(int sockNum) noexcept;
    explicit Socket(const std::string &address, int portNum, SocketType = SocketType::ETHERNET) noexcept;

    Socket(const Socket &)            = delete;
    Socket(Socket &&)                 = delete;
    Socket operator=(const Socket &s) = delete;
    Socket operator=(Socket &&)       = delete;
    ~Socket();

    bool open([[maybe_unused]] OpenMode mode = OpenMode::READ_WRITE) override;
    bool close() override;
    bool isOpened() override;
    int  bytesAviable() override;
    int  read(std::vector< uint8_t > &, int size = -1) override;
    int  write(std::vector< uint8_t > &, int size = -1) override;
    int  write(const DatatPackage &);

    std::shared_ptr< Socket > accept();
    bool                      connect();
    bool                      nonBlockingMode();
    int                       getFd() const;

    void setMaximumConnectionsHandle(int maxConnections);

  private:
    bool listen();

    int  toNixSocketType() const noexcept;
    bool makeBindLocal() noexcept;
    bool makeBindInet() noexcept;
    void handleError(const std::string &someMessage);

  private:
    bool        asycnSocket_ { false };
    int         maxConnections_ = 5;
    int         socketPortNum_ { -1 };
    int         sock_ { -1 };
    SocketType  sockType_ { SocketType::ETHERNET };
    std::string socketAddress_ {};
};

using SocketPtr = std::shared_ptr< Socket >;

#endif  // SOCKET_H
