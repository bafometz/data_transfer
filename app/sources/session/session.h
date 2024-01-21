#ifndef SESSION_H
#define SESSION_H
#include "../data_package/datatpackage.h"
#include "../time/time.h"
#include <cstdlib>
#include <fstream>

struct data_transmitted
{
    uint64_t packageSizeInBytes = 0;  ///< Максимальный размер одного пакета для передачи
    uint64_t maxPackages        = 0;  ///< Всего пакетов должно быть передано
    uint64_t packagesRecived    = 0;  ///< Передано пакетов за сессию
    uint64_t bytesRecived       = 0;  ///< Байт получено за сессию
    uint64_t maxBytes           = 0;  ///< Максимальное количество ожидаемых байт

    /**
     * @brief Конвертирует размер файла в кол-во ожидаемых пакетов
     * @param Размер пакета
     */
    uint64_t convertBytesToPackages(uint64_t fileSize)
    {
        if (fileSize < 1024)
        {
            maxPackages = 1;
            maxBytes    = fileSize;
            return maxPackages;
        }
        else if (fileSize < (1024 * 1024))
        {
            packageSizeInBytes = 1024;
        }
        else
        {
            packageSizeInBytes = 2048;
        }

        auto division = div(fileSize, packageSizeInBytes);
        if (division.rem > 0) division.quot++;
        maxPackages = division.quot;
        maxBytes    = fileSize;
        return maxPackages;
    };

    /**
     * @brief Считает пакеты и увеличивает счетчик
     *
     */
    void packageRecived(uint64_t packageSize)
    {
        packagesRecived++;
        bytesRecived += packageSize;
    }

    void resetFields()
    {
        packageSizeInBytes = 0;
        maxPackages        = 0;
        packagesRecived    = 0;
        bytesRecived       = 0;
        maxBytes           = 0;
    }
};

class Session
{
  public:
    Session();
    ~Session();
    void              reset();
    void              setPathToFile(const std::string& pathWhereSaveFile);
    bool              openFile();
    bool              writeToFile(const data_buffer&, size_t bytesToWrite);
    bool              canSaveFile();
    void              printInfo();
    void              calcPackages();
    std::string       fileName() const;
    data_buffer&      bufferRef();
    data_transmitted& transmittedDataRef();
    DatatPackage&     lastSendedPackageRef();
    DatatPackage&     packageToSendRef();
    DatatPackage&     recivedPackageRef();

  private:
    DateTime         dateTime_;
    data_buffer      buffer_;
    std::fstream     fileToSave_;
    std::string      connectionTime_;
    std::string      pathToFile_;
    data_transmitted transmittedData_;
    Timer            timer_;
    DatatPackage     lastSendedPackage_;
    DatatPackage     packageToSend_;
    DatatPackage     recivedPackage_;
};

#endif  // SESSION_H
