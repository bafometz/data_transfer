#ifndef CLIENT_H
#define CLIENT_H
#include "../data_package/datatpackage.h"
#include "../socket/socket.h"
#include <string>

class Client
{
  public:
    explicit Client(const std::string& address, int port);

    int sendFile(const std::string& filePath);

  private:
    int getfileSize(const std::string& file) const;

    std::pair< uint64_t, uint64_t > requestSendData(int fileSizeInBytes);
    int                             readAndSendFile(const std::string& file, std::pair< uint64_t, uint64_t >);
    bool                            confirmExit();

    bool retryPackage(const DatatPackage& pkg, DatatPackage& reply, int times);

  private:
    int         port_;
    int         buffSize_ = 1024;
    const int   maxRetry_ = 10;
    std::string address_;
    Socket      sock_;
};

#endif  // CLIENT_H
