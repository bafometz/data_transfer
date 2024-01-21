#include "session.h"
#include "../helpers/helpers.h"
#include "../logger/logger.h"

Session::Session() :
    connectionTime_ { dateTime_.getCurrentTimestampStr() },
    pathToFile_ { helpers::getDir(helpers::pathToExec()) }
{
    timer_.start();
}

Session::~Session()
{
    timer_.stop();
}

void Session::reset()
{
    fileToSave_.close();
    connectionTime_ = dateTime_.getCurrentTimestampStr();
    transmittedData_.resetFields();
    timer_.stop();
    helpers::removeFile(pathToFile_ + "/" + connectionTime_ + ".hex");
}

void Session::setPathToFile(const std::string &pathWhereSaveFile)
{
    pathToFile_ = pathWhereSaveFile;
}

bool Session::openFile()
{
    fileToSave_.open(pathToFile_ + "/" + connectionTime_, std::ios::binary | std::ios::out | std::ios::trunc);
    return fileToSave_.is_open();
}

bool Session::writeToFile(const data_buffer &buff, size_t bytesToWrite)
{
    if (!fileToSave_.is_open()) return false;
    if (buff.size() < bytesToWrite) return false;
    if (helpers::getFreeDiskSpace(pathToFile_) < bytesToWrite) return false;
    fileToSave_.write(( char * )buff.data(), bytesToWrite);
    return true;
}

bool Session::canSaveFile()
{
    if (pathToFile_.empty())
    {
        LOG_ERROR("Can't save file, path to file is not setted");
        return false;
    }

    if (helpers::getFreeDiskSpace(pathToFile_) < transmittedData_.maxBytes)
    {
        LOG_ERROR("Can't save file");
        LOG_ERROR("File size: ", transmittedData_.maxBytes);
        LOG_ERROR("Free space at path", pathToFile_, "=", helpers::getFreeDiskSpace(pathToFile_));
        return false;
    }
    return true;
}

void Session::printInfo()
{
    LOG_INFO("Session info:");
    LOG_INFO("Session start at: ", connectionTime_);
    LOG_INFO("Current timestamp:", dateTime_.getTimestampStr(dateTime_.getMsSinceEpoh()));
    LOG_INFO("Session duration:", timer_.getLap(), "ms");
    LOG_INFO("Bytes recived:", transmittedData_.bytesRecived);
}

std::string Session::fileName() const
{
    return connectionTime_ + ".hex";
}

data_buffer &Session::bufferRef()
{
    return buffer_;
}

data_transmitted &Session::transmittedDataRef()
{
    return transmittedData_;
}

DatatPackage &Session::lastSendedPackageRef()
{
    return lastSendedPackage_;
}

DatatPackage &Session::packageToSendRef()
{
    return packageToSend_;
}

DatatPackage &Session::recivedPackageRef()
{
    return recivedPackage_;
}
