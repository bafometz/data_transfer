#ifndef TRANSMITTIONSTATUS_H
#define TRANSMITTIONSTATUS_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include "../data_package/datatpackage.h"
#include "../helpers/helpers.h"

enum class TRANSMISSION_STATE
{
    AWAIT_FILE_SIZE,      ///< Ожидает приём файла
    RECIVE_FILE,          ///< Идёт приём файла
    AWAIT_FINAL_MESSAGE,  ///< Ожидает подтверждение об успешном приёме файла
    ABORT,                ///<  Разрыв соединения
};

struct time_handler
{
    time_handler()                               = default;
    ~time_handler()                              = default;
    time_handler(const time_handler& th)         = delete;
    time_handler(time_handler&&)                 = delete;
    time_handler& operator=(const time_handler&) = delete;
    time_handler& operator=(time_handler&&)      = delete;

    void start() { timepointStart = std::chrono::steady_clock::now(); };

    uint64_t stop()
    {
        using namespace std::chrono;
        auto stop       = steady_clock::now();
        auto difference = stop - timepointStart;
        return duration_cast< milliseconds >(difference).count();
    };

    void updateCreationTime()
    {
        using namespace std::chrono;
        creationTime = system_clock::now();
    }

  private:
    std::chrono::steady_clock::time_point timepointStart;
    std::chrono::system_clock::time_point creationTime = std::chrono::system_clock::now();
};

struct transmit_state
{
    transmit_state() { buffer.reserve(rwChunkSize); };

    void cleanUp()
    {
        if (inputFile.is_open() && std::filesystem::exists(inputFilePath + "/" + inputFileName) && packagesRecived != packagesTotal)
        {
            inputFile.close();
            helpers::removeFile(inputFilePath + "/" + inputFileName);
        }
    }

    const int          rwChunkSize = DatatPackage::maxDataSize();
    TRANSMISSION_STATE state { TRANSMISSION_STATE::AWAIT_FILE_SIZE };  ///< Статус передачи данных

    uint64_t handleTimestamp { 0 };   ///< Время с начала эпохи в мс, для конвертации в имя файла
    uint64_t packagesRecived { 0 };   ///< Сколько пакетов принято
    uint64_t packagesTotal { 0 };     ///< Сколько пакетов ожидается
    uint64_t diskSpaceAviable { 0 };  ///< Свободное место на диске

    std::string inputFileName { "" };  ///< Итоговое имя файла
    std::string inputFilePath { "" };  ///< Путь к месту для сохранения файла

    std::fstream inputFile {};  ///< сам файл

    time_handler time;

    data_buffer  buffer;
    DatatPackage packageToSend;
    DatatPackage lastSendedPackage;
};
#endif  // TRANSMITTIONSTATUS_H
