#ifndef TRANSMITTIONSTATUS_H
#define TRANSMITTIONSTATUS_H
#include "../data_package/datatpackage.h"
#include "../helpers/helpers.h"
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

enum class TRANSMISSION_STATE
{
    AWAIT_FILE_SIZE,
    RECIVE_FILE,
    AWAIT_FINAL_MESSAGE,
    ABORT,
};

struct time_handler
{
    time_handler() = default;

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

    std::string getCreationDate()
    {
        using namespace std::chrono;
        auto               ms    = duration_cast< milliseconds >(creationTime.time_since_epoch()) % 1000;
        auto               timer = system_clock::to_time_t(creationTime);
        std::tm            bt    = *std::localtime(&timer);
        std::ostringstream oss;
        oss << std::put_time(&bt, "%d-%m-%Y_%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }

  private:
    std::chrono::steady_clock::time_point timepointStart;
    std::chrono::system_clock::time_point creationTime = std::chrono::system_clock::now();
    ;
};

struct transmit_state
{
    transmit_state() { buffer.reserve(rwChunkSize); };

    void cleanUp()
    {
        if (inputFile.is_open() && helpers::isFileExist(inputFilePath + "/" + inputFileName) && packagesRecived != packagesTotal)
        {
            inputFile.close();
            helpers::removeFile(inputFilePath + "/" + inputFileName);
        }
    }

    const int          rwChunkSize = DatatPackage::maxDataSize();
    TRANSMISSION_STATE state { TRANSMISSION_STATE::AWAIT_FILE_SIZE };  ///< Статус

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
