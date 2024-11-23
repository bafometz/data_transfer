#include "server.h"
#include "../helpers/helpers.h"
#include "../logger/logger.h"
#include "../session/session.h"
#include <cstring>
#include <sys/epoll.h>

Server::Server(int port) :
    port_ { port }
{
    std::signal(SIGINT, SignalHandler::signalHandler);
    std::signal(SIGKILL, SignalHandler::signalHandler);
    std::signal(SIGTERM, SignalHandler::signalHandler);
    std::signal(SIGABRT, SignalHandler::signalHandler);
    std::signal(SIGPIPE, SignalHandler::signalHandler);
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
            Session        ss;
            st.buffer.resize(2048);
            recivePackage(lp, st, pSock, ss);
            sendPackage(lp, st, pSock, ss);
            if (!lp.initEventPoll()) return;
            lp.start();
        });
}

void Server::recivePackage(EventLoop& ev, transmit_state& state, SocketPtr pSock, Session& ss)
{
    ev.bindSlot(EPOLLIN,
                [&state, pSock, &ss]()
                {
                    auto recivedDataSize = pSock->read(state.buffer, state.rwChunkSize);
                    ss.recivedPackageRef().replacePackage(state.buffer);
                    LOG_INFO("Recived from client:", recivedDataSize, "bytes");

                    if (recivedDataSize < 0)  // Ошибка, отвалился клиент (т.к. принятые данные -1)
                    {
                        LOG_ERROR("Less than zero data readed, error");
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (recivedDataSize == 0)  // Ошибка, ничего не прочитали от клиента, получается тоже отвалился
                    {
                        LOG_ERROR("0 bytes from client socket read, close socket");
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (recivedDataSize < DatatPackage::minSize())  // Ошибка, количество полученных байт меньше минимально допустимого
                    {
                        LOG_ERROR("Package smaller than 8 bytes, error");
                        ss.packageToSendRef().clearData();
                        ss.packageToSendRef().setCommand(COMMAND::CHECKSUM_ERROR);
                        ss.packageToSendRef().calcChecksum();
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    if (!ss.recivedPackageRef().verifyCheckSum())  // Ошибка контрольной суммы пакета, нужно уведомить клиента
                    {
                        LOG_INFO("Checksum error");
                        ss.packageToSendRef().clearData();
                        ss.packageToSendRef().setCommand(COMMAND::CHECKSUM_ERROR);
                        ss.packageToSendRef().calcChecksum();
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    if (ss.recivedPackageRef().getCommand() == COMMAND::CHECKSUM_ERROR)  // Клиенту пришел битый пакет, нужно отправить заново
                    {
                        LOG_WARN("Client recive broken package, resend");
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    if (state.state == TRANSMISSION_STATE::AWAIT_FILE_SIZE)
                    {
                        // Получаем размер файла
                        std::vector< uint8_t > fSizeArray(8);
                        ss.recivedPackageRef().getData(fSizeArray);

                        // Устанавливаем размер файла
                        ss.transmittedDataRef().maxBytes = fromBytes< uint64_t >(fSizeArray);

                        // Проверяем, есть ли возможность сохранить файл, если нет - прервыаем передачу
                        if (!ss.canSaveFile())
                        {
                            LOG_ERROR("Can't save file, path to save files empty");
                            ss.packageToSendRef().setCommand(COMMAND::REQUEST_TO_SEND_REJECT);
                            state.state = TRANSMISSION_STATE::ABORT;
                            state.packageToSend.replacePackage(std::move(ss.packageToSendRef()));
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        LOG_INFO("Generated file name", ss.fileName());
                        ss.transmittedDataRef().convertBytesToPackages(ss.transmittedDataRef().maxBytes);

                        LOG_INFO("Server await", ss.transmittedDataRef().maxPackages, "packages");
                        LOG_INFO("Package size", ss.transmittedDataRef().packageSizeInBytes, "packages");
                        ss.packageToSendRef().setCommand(COMMAND::REQUEST_TO_SEND_APPROVED);
                        std::vector< uint8_t > total_packages = toBytes< std::vector< uint8_t > >(( uint64_t )ss.transmittedDataRef().maxPackages);
                        std::vector< uint8_t > onePackageSize =  // Размер одного пакета
                            toBytes< std::vector< uint8_t > >(( uint64_t )ss.transmittedDataRef().packageSizeInBytes);

                        // Сливаем два блока данных в один
                        //  первые 64 байта - сколько пакетов ожидается
                        //  вторые 64 байта - размер одного пакета
                        ss.bufferRef().clear();
                        ss.packageToSendRef().clearData();
                        ss.bufferRef().insert(ss.bufferRef().begin(), total_packages.begin(), total_packages.end());
                        ss.bufferRef().insert(ss.bufferRef().begin() + total_packages.size(), onePackageSize.begin(), onePackageSize.end());
                        ss.packageToSendRef().setData(ss.bufferRef());
                        ss.packageToSendRef().calcChecksum();

                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::RECIVE_FILE)
                    {
                        if (!ss.openFile())
                        {
                            LOG_ERROR("Can't open file");
                            state.state = TRANSMISSION_STATE::ABORT;
                            ss.packageToSendRef().setCommand(COMMAND::ABORT);
                            ss.packageToSendRef().clearData();
                            ss.packageToSendRef().calcChecksum();
                            state.packageToSend.replacePackage(std::move(ss.packageToSendRef()));
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        if (ss.recivedPackageRef().getCommand() != COMMAND::DATA_PACKAGE)
                        {
                            ss.packageToSendRef().setCommand(COMMAND::ABORT);
                            ss.packageToSendRef().clearData();
                            ss.packageToSendRef().calcChecksum();
                            return EVENT_LOOP_SIGNALS::SIG_NONE;
                        }

                        ss.bufferRef().clear();
                        auto bytesToWrite = ss.recivedPackageRef().getData(ss.bufferRef());
                        ss.writeToFile(ss.bufferRef(), bytesToWrite);

                        ss.transmittedDataRef().packageRecived(bytesToWrite);
                        ss.printInfo();
                        ss.packageToSendRef().clearData();
                        ss.packageToSendRef().setCommand(COMMAND::PACKAGE_ACCPTED);
                        ss.packageToSendRef().setData(state.packagesRecived);
                        ss.packageToSendRef().calcChecksum();
                    }
                    else if (state.state == TRANSMISSION_STATE::AWAIT_FINAL_MESSAGE)
                    {
                        if (ss.recivedPackageRef().getCommand() == COMMAND::ALL_DATA_SENDED)
                        {
                            LOG_INFO("The client confirmed successful data transfer");
                            LOG_INFO("Close connection");
                            ss.printInfo();
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

void Server::sendPackage(EventLoop& ev, transmit_state& state, SocketPtr pSock, Session& ss)
{
    ev.bindSlot(EPOLLOUT,
                [&state, pSock, &ss]() -> EVENT_LOOP_SIGNALS
                {
                    if (ss.packageToSendRef().getCommand() == COMMAND::EMPTY_CMD)
                    {
                        // LOG_CRITICAL("Command to send: COMMAND::EMPTY_CMD");
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }

                    auto writeResult = pSock->write(ss.packageToSendRef());

                    if (writeResult < 0)
                    {
                        LOG_ERROR("Write result less than zero, exit");
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    if (writeResult == 0)
                    {
                        LOG_ERROR("Send responce to client error, 0 bytes written, abort");
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }

                    ss.lastSendedPackageRef().replacePackage(std::move(ss.packageToSendRef()));
                    ss.packageToSendRef().setCommand(COMMAND::EMPTY_CMD);

                    if (state.state == TRANSMISSION_STATE::AWAIT_FILE_SIZE)  // Ждём первую посылку от клиента
                    {
                        state.state = TRANSMISSION_STATE::RECIVE_FILE;
                        return EVENT_LOOP_SIGNALS::SIG_NONE;
                    }
                    else if (state.state == TRANSMISSION_STATE::RECIVE_FILE)  // Находимся в состоянии приёма файла
                    {
                        if (ss.transmittedDataRef().maxPackages == ss.transmittedDataRef().packagesRecived)  // Получили все ождидаемые пакеты
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
                        LOG_WARN("Abort connection with client");
                        pSock->write(state.packageToSend);
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }
                    else
                    {
                        LOG_WARN("Unknown state");
                        ss.reset();
                        return EVENT_LOOP_SIGNALS::SIG_EXIT;
                    }
                });
}
