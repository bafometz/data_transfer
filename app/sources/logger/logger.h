#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <mutex>
#include <string>
#include <utility>

struct source_location
{
    std::string filename = "";
    std::string funcname = "";
    int         line     = 0;

    friend std::ostream &operator<<(std::ostream &out, const source_location &loc)
    {
        out << loc.filename << ":" << loc.line;
        return out;
    }
};
enum class LOG_LEVEL
{
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERR,
    CRITICAL,
    OFF,
    N_LEVELS
};

class Logger
{
    using locker = std::lock_guard< std::mutex >;

  public:
    static Logger &getInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger(const Logger &)  = delete;
    Logger(const Logger &&) = delete;

    Logger &operator=(const Logger &)  = delete;
    Logger &operator=(const Logger &&) = delete;

  private:
  public:
    template< typename... Args >
    void log(int logLvl, [[maybe_unused]] const std::string& sinkName, [[maybe_unused]] const source_location &location,
             [[maybe_unused]] Args &&...args)
    {
        switch (logLvl)
        {
        case 0:
            {
                locker _(traceMutex);
                std::cout << lightBlue << "[TRACE] " << location << " " << location.funcname << " ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        case 1:
            {
                locker _(debugMutex);
                std::cout << blue << "[DEBUG] " /*<< location << " "*/;
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        case 2:
            {
                locker _(infoMutex);
                std::cout << green << "[INFO] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        case 3:
            {
                locker _(warnMutex);
                std::cout << yellow << "[WARN] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        case 4:
            {
                locker _(errorMutex);
                std::cout << lightred << "[ERROR] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        case 5:
            {
                locker _(criticalMutex);
                std::cout << red << "[CRITICAL] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << reset << std::endl;
                return;
            }
        default:
            return;
        }
    }

  private:
    Logger() {};

    const std::string red { "\033[1;31m" };
    const std::string lightred { "\033[91m" };
    const std::string green { "\033[1;32m" };
    const std::string yellow { "\033[1;33m" };
    const std::string cyan { "\033[0;36m" };
    const std::string magenta { "\033[1;35m" };
    const std::string blue { "\033[34m" };
    const std::string lightBlue { "\033[94m" };
    const std::string reset { "\033[0m" };
    std::string       lastOutputStr {};

    std::mutex strMutex;
    std::mutex traceMutex;
    std::mutex debugMutex;
    std::mutex infoMutex;
    std::mutex warnMutex;
    std::mutex errorMutex;
    std::mutex criticalMutex;
};

static Logger &loggerInstance = Logger::getInstance();

#define LOG_TRACE(...) loggerInstance.log(0, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define LOG_DEBUG(...) loggerInstance.log(1, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)

#define LOG_INFO(...)     loggerInstance.log(2, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define LOG_WARN(...)     loggerInstance.log(3, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define LOG_ERROR(...)    loggerInstance.log(4, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define LOG_CRITICAL(...) loggerInstance.log(5, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)

#endif  // LOGGER_H
