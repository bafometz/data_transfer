#include "server.h"
#include "../helpers/helpers.h"
#include "../logger/logger.h"

#include <cstring>
#include <sys/epoll.h>

Server::Server(int port) :
    port_ { port }
{
    std::signal(SIGINT, SignalHandler::signalHandler);
    std::signal(SIGKILL, SignalHandler::signalHandler);
    std::signal(SIGTERM, SignalHandler::signalHandler);
    std::signal(SIGABRT, SignalHandler::signalHandler);
}

Server::~Server()
{
    SignalHandler::instance().disableAtomic();

    if (masterSocket_)
    {
        masterSocket_->close();
    }
}

int Server::start()
{
    if (!openConnection())  // Открываем сокет
    {
        return -1;
    }

    EventLoop lp(EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLERR, masterSocket_->getFd());

    lp.initEventPoll();

    lp.bindSlot(EPOLLIN,
                [this]()
                {
                    auto sock = acceptNewConnection();
                    if (!sock) return EVENT_LOOP_SIGNALS::SIG_NONE;

                    createSubEventLoop(sock);
                    return EVENT_LOOP_SIGNALS::SIG_NONE;
                });
    lp.start();
    return 0;
}

bool Server::openConnection()
{
    auto pSock = new Socket("0.0.0.0", port_);
    pSock->setMaximumConnectionsHandle(maxEventsConnectionToHandle_);

    if (!pSock->open())
    {
        LOG_ERROR("Can't start server at localhost", port_);
        return false;
    }

    LOG_INFO("Listening 0.0.0.0:", port_);
    masterSocket_ = std::shared_ptr< Socket >(pSock);
    masterSocket_->nonBlockingMode();
    return true;
}

SocketPtr Server::acceptNewConnection()
{
    SocketPtr newSock = masterSocket_->accept();

    if (newSock)
    {
        newSock->nonBlockingMode();
        return newSock;
    }
    else
    {
        LOG_ERROR("Can't accept connection to server", std::strerror(errno));
        return nullptr;
    }

    return nullptr;
}

void Server::createSubEventLoop(SocketPtr pSock)
{
    tp.enqueue(
        [pSock, this]()
        {
            EventLoop      lp(EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR, pSock->getFd());
            transmit_state st;
            st.buffer.resize(2048);
            recivePackage(lp, st, pSock);
            sendPackage(lp, st, pSock);
            if (!lp.initEventPoll()) return;
            lp.start();
        });
}

void Server::recivePackage(EventLoop& ev, transmit_state& state, SocketPtr pSock)
{
    ev.bindSlot(EPOLLIN,
                [&state, pSock, &ev]()
                {
                    DatatPackage recivePackage;
                    DatatPackage responsePackage;

                    auto recivedDataSize = pSock->read(state.buffer, state.rwChunkSize);
                    recivePackage.replacePackage(state.buffer);

                    if (recivedDataSize < 0)  // Ошибка 1
                    {
                        LOG_ERROR("Less than zero data readed, error");
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (recivedDataSize == 0)  // Ошибка 2
                    {
                        LOG_ERROR("0 bytes from client socket read, close socket");
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (recivedDataSize < DatatPackage::minSize())  // Ошибка 3
                    {
                        LOG_ERROR("Package smaller than 8 bytes, error");
                        responsePackage.setCommand(COMMAND::CHECKSUM_ERROR);
                        responsePackage.calcChecksum();
                        state.packageToSend.replacePackage(std::move(responsePackage));
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    if (!recivePackage.verifyCheckSum())  // Ошибка 4
                    {
                        LOG_INFO("Checksum error");
                        responsePackage.setCommand(COMMAND::CHECKSUM_ERROR);
                        responsePackage.calcChecksum();
                        state.packageToSend.replacePackage(std::move(responsePackage));
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    LOG_INFO("Recived from client:", recivedDataSize, "bytes");

                    if (recivePackage.getCommand() == COMMAND::CHECKSUM_ERROR)
                    {
                        LOG_WARN("Client recive broken package, resend");
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    if (state.state == TRANSMISSION_STATE::AWAIT_FILE_SIZE)
                    {
                        // Получаем размер файла
                        std::vector< uint8_t > fSizeArray(8);
                        recivePackage.getData(fSizeArray);
                        auto fileSize = fromBytes< uint64_t >(fSizeArray);

                        // Проверяем что есть место его схранить
                        state.inputFilePath = helpers::getDir(helpers::pathToExec());

                        if (state.inputFilePath.empty())
                        {
                            LOG_ERROR("Can't save file, path to save files empty");
                            responsePackage.setCommand(COMMAND::REQUEST_TO_SEND_REJECT);
                            state.state = TRANSMISSION_STATE::ABORT;
                            state.packageToSend.replacePackage(std::move(responsePackage));
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        auto freeSpace = helpers::getFreeDiskSpace(state.inputFilePath);

                        if (freeSpace < fileSize)
                        {
                            // Ответить ошибкой что всё плохо, места нет
                            LOG_ERROR("Can't save file, not enought disk space");
                            responsePackage.setCommand(COMMAND::REQUEST_TO_SEND_REJECT);
                            state.state = TRANSMISSION_STATE::ABORT;
                            state.packageToSend.replacePackage(std::move(responsePackage));
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        state.inputFileName = state.time.getCreationDate() + ".hex";

                        LOG_INFO("Generated file name", state.inputFileName);
                        if (helpers::isFileExist(state.inputFilePath + "/" + state.inputFileName))
                        {
                            LOG_WARN("File exist");
                            if (!helpers::removeFile(state.inputFilePath + "/" + state.inputFileName))
                            {
                                LOG_WARN("Can't delete old file");
                                state.time.updateCreationTime();
                                state.inputFileName = state.time.getCreationDate() + ".hex";
                                LOG_INFO("Rename file: ", state.inputFileName);
                            }
                        }

                        // Считаем кол-во пакетов
                        auto division = div(fileSize, 1024);
                        if (division.rem > 0)
                        {
                            division.quot++;
                        }

                        state.packagesTotal = division.quot;
                        LOG_INFO("Server await", state.packagesTotal, "packages");
                        responsePackage.setCommand(COMMAND::REQUEST_TO_SEND_APPROVED);
                        std::vector< uint8_t > total = toBytes< std::vector< uint8_t > >(( uint64_t )state.packagesTotal);
                        responsePackage.setData(total);
                        responsePackage.calcChecksum();
                        state.packageToSend.replacePackage(std::move(responsePackage));
                        LOG_INFO("Ready to write file");
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::RECIVE_FILE)
                    {
                        if (!state.inputFile.is_open())
                        {
                            state.inputFile.open(state.inputFilePath + "/" + state.inputFileName, std::ios::binary | std::ios::out | std::ios::trunc);

                            if (!state.inputFile.is_open())
                            {
                                LOG_ERROR("Can't open file");
                                state.state = TRANSMISSION_STATE::ABORT;
                                responsePackage.setCommand(COMMAND::ABORT);
                                responsePackage.calcChecksum();
                                state.packageToSend.replacePackage(std::move(responsePackage));
                                return EVENT_LOOP_SIGNALS::SIG_NONE;
                            }
                        }

                        if (recivePackage.getCommand() != COMMAND::DATA_PACKAGE)
                        {
                            // мб тут ошибку какую-то отправлять
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        state.buffer.clear();
                        auto bytesToWrite = recivePackage.getData(state.buffer);
                        state.inputFile.write(( const char* )state.buffer.data(), bytesToWrite);
                        state.packagesRecived++;
                        LOG_INFO("Recived: ", state.packagesRecived, "/", state.packagesTotal);

                        responsePackage.setCommand(COMMAND::PACKAGE_ACCPTED);
                        responsePackage.setData(state.packagesRecived);
                        responsePackage.calcChecksum();
                        state.packageToSend.replacePackage(std::move(responsePackage));
                    }
                    else if (state.state == TRANSMISSION_STATE::AWAIT_FINAL_MESSAGE)
                    {
                        if (recivePackage.getCommand() == COMMAND::ALL_DATA_SENDED)
                        {
                            LOG_INFO("The client confirmed successful data transfer");
                            LOG_INFO("Close connection");
                            return EVENT_LOOP_SIGNALS::SIG_EXIT;
                        }
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }
                    else
                    {
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    return EVENT_LOOP_SIGNALS::SIG_NONE;
                });

    ev.bindSlot(EPOLLRDHUP, []() { return EVENT_LOOP_SIGNALS::SIG_EXIT; });
    ev.bindSlot(EPOLLERR, []() { return EVENT_LOOP_SIGNALS::SIG_EXIT; });
    ev.bindSlot(EPOLLHUP, []() { return EVENT_LOOP_SIGNALS::SIG_EXIT; });
}

void Server::sendPackage(EventLoop& ev, transmit_state& state, SocketPtr pSock)
{
    ev.bindSlot(EPOLLOUT,
                [&state, pSock, &ev]() -> EVENT_LOOP_SIGNALS
                {
                    if (state.packageToSend.getCommand() == COMMAND::EMPTY_CMD)
                    {
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    auto writeResult = pSock->write(state.packageToSend);
                    if (writeResult < 0)
                    {
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (writeResult == 0)
                    {
                        LOG_ERROR("Send recponce to client error, 0 bytes written, abort");
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    state.lastSendedPackage.replacePackage(std::move(state.packageToSend));
                    state.packageToSend.setCommand(COMMAND::EMPTY_CMD);

                    if (state.state == TRANSMISSION_STATE::AWAIT_FILE_SIZE)
                    {
                        state.state = TRANSMISSION_STATE::RECIVE_FILE;
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::RECIVE_FILE)
                    {
                        if (state.packagesRecived == state.packagesTotal)
                        {
                            state.state = TRANSMISSION_STATE::AWAIT_FINAL_MESSAGE;
                        }
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::AWAIT_FINAL_MESSAGE)
                    {
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::ABORT)
                    {
                        auto writeREs = pSock->write(state.packageToSend);
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }
                    else
                    {
                        state.cleanUp();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }
                });
}
