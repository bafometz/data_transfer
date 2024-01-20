#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <vector>

#include "../data_package/datatpackage.h"
#include "../io_device/iodevice.h"
#include <memory>

/**
 * @brief Класс сокета, реализовывает в себе не все типы сокетов, только локальный и сетевой сокерт и только tcp
 */
class Socket : public IODevice
{
    /**
     * @brief Тип сокета
     */
    enum class SocketType
    {
        LOCAL,     ///< Локлальный
        ETHERNET,  ///< Сетевой
    };

  public:
    /**
     * @brief Создаёт мастер сокет для подключений, остальные сокеты будут пораждаться от мастер сокета (man accept)
     * @param Адресс сервера
     * @param Порт к которому подключаться
     * @param Тип сокета
     * @param Блокирующий или неблокирующий сокет
     */
    explicit Socket(const std::string &address, int portNum, SocketType = SocketType::ETHERNET, bool nonBlockingMode = false) noexcept;

    /**
     * @brief Конструктор для пораждённых сокетов, нужно после подключения, т.к. даёт возможность читать/писать + реализует RAII
     */
    explicit Socket(int sockNum) noexcept;

    Socket(const Socket &)            = delete;
    Socket(Socket &&)                 = delete;
    Socket operator=(const Socket &s) = delete;
    Socket operator=(Socket &&)       = delete;

    /**
     * @brief Закроет сокет, если он был не закрыт по какой-то причине
     */
    ~Socket();

    /**
     * @brief Переопределённая функция виртуального метода
     * @param Не используются
     * @return Возвращает false если не получилось открыть сокет, ошибка будет напечатана в консоль
     */
    bool open([[maybe_unused]] OpenMode mode = OpenMode::READ_WRITE) override;

    /**
     * @brief Закрывает сокет и переводит значение файлового дескриптора в -1
     * @return Возвращает false  в случае ошибки, возвращаемый параметр можно игнорировать
     */
    bool close() override;

    /**
     * @brief Провеяет открыт ли сокет, по факту проверка на -1
     */
    bool isOpened() override;

    /**
     * @brief Возвращает количество байт доступных для чтения
     * @return Количество байт достпных для чтения
     */
    int bytesAviable() override;
    /**
     * @brief Смотри IODevice
     */
    int read(std::vector< uint8_t > &, int size = -1) override;

    /**
     * @brief Смотри IODevice
     */
    int write(std::vector< uint8_t > &, int size = -1) override;

    /**
     * @brief Записывает данные из переданной структуры в сокет
     * @param DataPackage - пакет с данными для передачи
     */
    int write(const DatatPackage &);

    /**
     * @brief Функция принимающая новое подключение, по-факту клонирует мастер-сокет и отдает новый, с соединением
     * вызов функции релевантен только для мастер-сокета с стороны сервера
     *
     * @return std::shared_ptr<Socket> - сокет-клон с помощью которого можно бощаться с клиентом
     */
    std::shared_ptr< Socket > accept();

    /**
     * @brief Функция для сокета клиента подключается к серверу в случае ошибки, ошибка будет отражена в консоле
     * @return true в случае успешного подключения, false  в случае ошибки
     */
    bool connect();

    /**
     * @brief По умолчанию сокеты создаются в блокирующем режиме, с помощью этой функции можно перевести сокет в неблокирующий режим работы
     * т.е. он не будет простаивать на функциях read/write
     */
    bool nonBlockingMode();

    /**
     * @brief Возвращает файловый дескриптор сокета
     * @return Файловый дескриптор
     */
    int getFd() const;

    /**
     * @brief Устанавливает максимально-допустимое количество подключений к мастер-сокету на сервере, по умолчанию в программе стоит 0
     */
    void setMaximumConnectionsHandle(int maxConnections);

  private:
    /**
     * @brief Запускает сокет на прослушку соединений, релевантно для мастер-сокета сервера (man listen)
     * @return true в случае успеха, false во всех остальных
     */
    bool listen();

    /**
     * @brief Конвертиреут перечисление SocketType в интовые значения флагов *NIX
     */
    int toNixSocketType() const noexcept;

    /**
     * @brief Для создания локального сокета
     * @warning Метод не работает
     * @todo  Закончить бы его
     */
    bool makeBindLocal() noexcept;

    /**
     * @brief Для создания сокета работающего через интернет
     * @return true в случае успеха, false во всех остальных
     */
    bool makeBindInet() noexcept;

    /**
     * @brief Обработчик errno
     * @param Сообщение которое необходимо вывести в консоль + расшифровка errno (если он был выставлен)
     */
    void handleError(const std::string &someMessage);

  private:
    bool       asycnSocket_ { false };              ///< Является ли сокет асинхронным
    int        maxConnections_ = 5;                 ///< Максимальное количество подключений к сокету
    int        socketPortNum_ { -1 };               ///< Порт на котором будет открыт сокет
    int        sock_ { -1 };                        ///< Файловый дескриптор сокета
    SocketType sockType_ { SocketType::ETHERNET };  ///< Тип сокета
    std::string socketAddress_ {};  ///< Aдрес на котором будет открыт сокет, для общения внутри локальной сети 0.0.0.0
};

using SocketPtr = std::shared_ptr< Socket >;

#endif  // SOCKET_H
