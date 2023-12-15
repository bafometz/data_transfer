#ifndef IODEVICE_H
#define IODEVICE_H
#include <cstdint>
#include <vector>

class IODevice
{
  public:
    enum class OpenMode
    {
        READ,
        WRITE,
        READ_WRITE
    };

    IODevice() = default;
    virtual ~IODevice() {};

    virtual bool open(OpenMode mode = OpenMode::READ_WRITE)    = 0;
    virtual bool close()                                       = 0;
    virtual bool isOpened()                                    = 0;
    virtual int  read(std::vector< uint8_t >&, int size = -1)  = 0;
    virtual int  write(std::vector< uint8_t >&, int size = -1) = 0;
    virtual int  bytesAviable()                                = 0;
};

#endif  // IODEVICE_H
