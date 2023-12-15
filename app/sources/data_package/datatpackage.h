#ifndef DATATPACKAGE_H
#define DATATPACKAGE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

using byte = uint8_t;

template< typename Container, typename T >
Container toBytes(T value)
{
    if constexpr (std::is_same< Container, std::vector< uint8_t > >::value)
    {
        std::vector< uint8_t > buffer(sizeof(T));
        std::copy_n(reinterpret_cast< uint8_t* >(&value), sizeof(T), buffer.begin());
        std::reverse(buffer.begin(), buffer.end());
        buffer.shrink_to_fit();
        return buffer;
    }
    else if (std::is_same< Container, std::array< uint8_t, sizeof(T) > >::value)
    {
        std::array< uint8_t, sizeof(T) > buffer;
        std::copy_n(reinterpret_cast< uint8_t* >(&value), sizeof(T), buffer.begin());
        std::reverse(buffer.begin(), buffer.end());
        return buffer;
    }
    else
    {
        static_assert("Supporte only vector<uint8_t> and array<uint8_t, N> ");
    }
}

template< typename T, typename Container >
T fromBytes(Container value)
{
    if constexpr (std::is_same< Container, std::vector< uint8_t > >::value)
    {
        std::reverse(value.begin(), value.end());
        T ret = *reinterpret_cast< const T* >(&value[0]);
        return ret;
    }
    else if (std::is_same< Container, std::array< uint8_t, sizeof(T) > >::value)
    {
        if constexpr (value.max_size() > sizeof(T))
        {
            static_assert("Container size vlue size greater then T");
        }

        std::array< uint8_t, sizeof(T) > buffer;
        std::reverse(value.begin(), value.end());
        T ret = *reinterpret_cast< const T* >(&value[0]);
        return ret;
    }
    else
    {
        static_assert("Can't parse bytes");
    }

    if (value.empty())
    {
        return 0;
    }

    std::reverse(value.begin(), value.end());
    T ret = *reinterpret_cast< const T* >(&value[0]);
    return ret;
}

enum class COMMAND
{
    EMPTY_CMD       = 0,
    REQUEST_TO_SEND = 1,       ///< Запрос на отправку данных (Клиент -> Сервер)
    REQUEST_TO_SEND_APPROVED,  ///< Сервер готов к приёму данных (Сервер -> Клиент)
    REQUEST_TO_SEND_REJECT,    ///< Cервер не может принять данные  (Сервер -> Клиент)
    MUST_BE_TRANSMMITTED,      ///< Cколько пакетов будет передано (Клиент-Сервер)
    PACKAGE_ACCPTED,           ///< Пакет принят и обработан  (Сервер -> Клиент)
    ALL_DATA_SENDED,           ///< Все пакеты переданы, можно завершать общение (Клиент-Сервер)
    DATA_PACKAGE,              ///< Пакет с данными
    CHECKSUM_ERROR,            ///< Ошибка контрольной суммы пакета, необходимо переслать пакет

    ABORT   = 244,
    UNKNOWN = 255,
};

using data_buffer = std::vector< uint8_t >;

/**
 * @brief Пакет для передачи данных между клиентом и сервером
 * выбранна такая структура для удобства передачи как по сокету, так и по
 */

class DatatPackage
{
  public:
    explicit DatatPackage();
    explicit DatatPackage(DatatPackage&&);
    explicit DatatPackage(const std::vector< uint8_t >& data);
    explicit DatatPackage(const DatatPackage& dp);

    /**
     * @brief Считает контрольную сумму пакета
     * @return true  если посчитанная контрольная сумма равна текущей
     */
    bool verifyCheckSum();

    /**
     * @brief Вычисляет контрольную сумму для всего пакета и сохраняет ее, в качестве контрольной суммы используется crc32
     */
    void calcChecksum();  ///< Вычесляет контрольную сумму пакета (BigEndian)

    /**
     * @brief Устанавливает команду для пакета
     */
    void setCommand(COMMAND cm);  ///< Устанавливает текущую команду для посылки

    /**
     * @brief Разбивает целое число в массив байт и кладет в раздел "данных" пакета (BigEndian)
     * @param Целое число, которое необходимо отправить
     */
    void setData(uint16_t data);  ///< Упаковывает в посылку значение разбитое на байты(производится конвертация в BE)

    /**
     * @brief Перемещает вектор байт "как есть" в массив данных, для последующей отправки
     * @param std::vector< uint8_t >&& data массив  байт
     */
    void setData(std::vector< uint8_t >&& data);

    /**
     * @brief Копирует вектор байт в массив данных(data_), для последующей отправки
     * @param std::vector< uint8_t >&& data массив  байт
     * @param Размер значащих байт
     */
    void setData(const std::vector< uint8_t >& data, int size = -1);

    /**
     * @brief Перемещает вектор байт "как есть" в массив данных, для последующей отправки
     * @param std::vector< uint8_t >&& data массив  байт
     */
    void replacePackage(DatatPackage&& pkg);

    /**
     * @brief Копирует вектор байт  в массив данных, для последующей отправки
     * @param std::vector< uint8_t >&& data массив  байт
     */
    void replacePackage(const std::vector< uint8_t >& data);

    /**
     * @brief Возвращает текущую команду
     * @return COMMAND
     */
    COMMAND getCommand() const;

    /**
     * @brief Позволяет получить указатель на память с данными, которые передавались/будут передваваться в пакете
     * @param Указатель на область памяти
     * @return 0 если нет данных, иначе значение от [1;max] колчичество байт
     */
    // int getData(uint8_t* ptr) const;

    int getData(std::vector< uint8_t >&) const;
    /**
     * @brief Функция получения данных одним значением
     * @return Целое значение из поля данных
     */
    int getData() const;

    /**
     * @brief Cоздаёт пакет для отправки совмещая все данные в один массив
     * @param Массив в который будут помещенны данные для отправки
     * @return Длина возвращенного масива
     */
    int generatePackage(std::vector< uint8_t >& data) const;
    /**
     * @brief Возвращает длину массива data_ исходя из данных в dataSize_
     * @return Размер секции data_
     */
    uint16_t dataSizeFromHeader() const;

    /**
     * @brief Возвращает ссылку на контрольную сумму, отладочный метод
     */
    std::array< uint8_t, 4 >& getCrc();

    /**
     * @brief Возвращает максимально возможное значение байт для поля "данные"
     *  @return Максимальное число для uint16_t
     */
    static uint16_t maxDataSize();

    /**
     * @brief Возвращает минимальо возможное значение байт для всего пакета, когда отсутствует поле data_
     *  @return sizeof(header_) + sizeof(packageCommand_) + 2 + 4
     */
    static uint64_t maxSize();

    /**
     * @brief Возвращает минимальо возможное значение байт для всего пакета, когда отсутствует поле data_
     *  @return sizeof(header_) + sizeof(packageCommand_) + 2 + 4
     */
    static uint16_t minSize();

    /**
     * @brief Генерирует обзорную таблицу
     */
    static void generateCrc32Table();

  private:
    /**
     * @brief Заполняет заголовок header_,packageCommand_,dataSize_
     */
    int fillHeader(const std::vector< uint8_t >& data);

    /**
     * @brief Вычисляет crc32 для пакета
     */
    void calcCrc32(std::array< uint8_t, 4 >& result);

  private:
    const uint8_t                  header_         = 0xAA;            // 1
    uint8_t                        packageCommand_ = 0x00;            // 1
    std::array< uint8_t, 2 >       dataSize_       = { 0x00, 0x00 };  // 2
    std::vector< uint8_t >         data_;                             // n
    std::array< uint8_t, 4 >       crc_;                              // 4
    static std::vector< uint32_t > table_;
};

#endif  // DATATPACKAGE_H
