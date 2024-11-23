#include "client.h"
#include "../data_package/datatpackage.h"
#include "../helpers/helpers.h"
#include "../logger/logger.h"
#include <cstdio>
#include <fstream>

#define LOG_TAG "client"

Client::Client(const std::string &address, int port) :
    port_ { port },
    address_ { address },
    sock_ { address, port }
{
}

int Client::sendFile(const std::string &filePath)
{
    auto fileSize = helpers::fileSize(filePath);
    // Socket                 sock(address_, port_);
    if (!sock_.connect())
    {
        return 1;
    }

    LOG_INFO("Client prepare send file: ", filePath);
    LOG_INFO("File size: ", fileSize);

    // Отправляем запрос на отправку файла, прикрепляем кол-во байт для отправки
    // Если сервер готов принять, то он отвечает одобрением и сколько пакетов ожидает
    auto packAwait = requestSendData(fileSize);

    if (packAwait.first <= 0 || packAwait.second <= 0)
    {
        LOG_ERROR("Server doesen't await data");
        return 1;
    }

    // Всё отправили ждем завершения
    auto packagesSended = readAndSendFile(filePath, packAwait);
    LOG_INFO("Total packages uploaded:", packagesSended);

    // Подтверждаем что всё хорошо
    std::ignore = confirmExit();
    return 0;
}

int Client::getfileSize(const std::string &file) const
{
    std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

std::pair< uint64_t, uint64_t > Client::requestSendData(int fileSizeInBytes)
{
    DatatPackage dp;
    dp.setCommand(COMMAND::REQUEST_TO_SEND);
    auto pkgData = toBytes< std::vector< uint8_t > >(( uint64_t )fileSizeInBytes);
    dp.setData(pkgData);
    dp.calcChecksum();

    std::vector< uint8_t > pack;
    std::ignore    = dp.generatePackage(pack);
    std::ignore    = sock_.write(dp);

    pack.clear();
    pack.resize(buffSize_);
    std::ignore = sock_.read(pack, buffSize_);
    DatatPackage recivePackage;
    DatatPackage sendPackage;
    recivePackage.replacePackage(pack);

    if (!recivePackage.verifyCheckSum())
    {
        LOG_ERROR("Checksum verification failed");
        sendPackage.setCommand(COMMAND::CHECKSUM_ERROR);
        auto res = retryPackage(sendPackage, recivePackage, 10);
        if (res <= 0) return { -1, -1 };
    }

    if (recivePackage.getCommand() != COMMAND::REQUEST_TO_SEND_APPROVED)
    {
        LOG_ERROR("On request to file size recive", static_cast< int >(recivePackage.getCommand()));
    }
    std::vector< uint8_t > totalPackArr;
    recivePackage.getData(totalPackArr);
    std::vector< uint8_t > total_packages(totalPackArr.begin(), totalPackArr.begin() + (totalPackArr.size() * 0.5) - 1);
    std::vector< uint8_t > one_package_size(totalPackArr.begin() + (totalPackArr.size() * 0.5), totalPackArr.end());

    return { fromBytes< uint64_t >(total_packages), fromBytes< uint64_t >(one_package_size) };
}

int Client::readAndSendFile(const std::string &file, std::pair< uint64_t, uint64_t > send_info)
{
    FILE      *fp             = std::fopen(file.c_str(), "r");
    const auto fileSize       = static_cast<uint64_t>(getfileSize(file));
    uint64_t   uploadedBytes  = 0;
    int        retryCount     = 0;
    int        packagesSended = 0;
    buffSize_                 = send_info.second;
    std::vector< uint8_t > fileReadBuffer(buffSize_);
    std::vector< uint8_t > packagesBuffer(buffSize_);
    DatatPackage           request;
    DatatPackage           responce;

    for (; uploadedBytes < fileSize && retryCount < maxRetry_;)
    {
        if (std::fseek(fp, static_cast< long int >(uploadedBytes), SEEK_SET) != 0)
        {
            LOG_CRITICAL("fseek() failed in file ", file);
            std::fclose(fp);
            return -1;
        }

        auto readRes = std::fread(fileReadBuffer.data(), sizeof(uint8_t), buffSize_, fp);
        LOG_INFO("Readed from file:", readRes);

        request.setCommand(COMMAND::DATA_PACKAGE);
        request.setData(fileReadBuffer, readRes);
        request.calcChecksum();

        auto writeRes = sock_.write(request);
        LOG_INFO("Written to server:", writeRes, "bytes");

        auto responceSize = sock_.read(packagesBuffer, buffSize_);

        if (responceSize < DatatPackage::minSize())
        {
            LOG_ERROR("Error on reading from server data");
            return -1;
        }

        responce.replacePackage(packagesBuffer);

        if (!responce.verifyCheckSum())  // Если на нашей стороне не сошлась чексумма
        {
            LOG_INFO("Checksum error when check recive package");

            DatatPackage checksumerr;
            checksumerr.setCommand(COMMAND::CHECKSUM_ERROR);

            if (!retryPackage(checksumerr, responce, maxRetry_))
            {
                LOG_CRITICAL("Too many checksum errors/errors, abort");
                std::fclose(fp);
                return -1;
            }
            else
            {
                retryCount = 0;
            }
        }

        if (responce.getCommand() == COMMAND::CHECKSUM_ERROR)
        {
            retryCount++;
            LOG_WARN("Server doesen't accept package, retry: ", retryCount);
            continue;
        }
        else if (responce.getCommand() == COMMAND::PACKAGE_ACCPTED)
        {
            uploadedBytes += readRes;
            LOG_INFO("Server accepted package");
            LOG_INFO("Sended", uploadedBytes, "/", fileSize);
            packagesSended++;
            retryCount = 0;
        }
        else if (responce.getCommand() == COMMAND::ABORT)
        {
            LOG_WARN("Server send abort package");
            return -1;
        }
        else
        {
            retryCount++;
            LOG_WARN("Unhandled respoce, retry:", retryCount, static_cast< int >(responce.getCommand()));
            continue;
        }
    }

    std::fclose(fp);

    if (retryCount == maxRetry_)
    {
        return -1;
    }
    else
    {
        return packagesSended;
    }
}

bool Client::confirmExit()
{
    DatatPackage request;
    request.setCommand(COMMAND::ALL_DATA_SENDED);
    std::vector< uint8_t > pack;
    request.calcChecksum();
    request.generatePackage(pack);
    auto writeRes = sock_.write(pack, pack.size());

    LOG_INFO("Written to server:", writeRes, "bytes");
    pack.clear();
    return true;
}

bool Client::retryPackage(const DatatPackage &pkg, DatatPackage &reply, int times)
{
    std::vector< uint8_t > pack(buffSize_);

    for (int i = 0; i < times; i++)
    {
        auto writeRes = sock_.write(pack, pack.size());
        if (writeRes <= 0)
        {
            return false;
        }

        std::ignore = sock_.read(pack, buffSize_);
        reply.replacePackage(pack);

        if (reply.getCommand() != COMMAND::UNKNOWN || reply.getCommand() != COMMAND::CHECKSUM_ERROR)
        {
            return true;
        }
    }

    return false;
}
