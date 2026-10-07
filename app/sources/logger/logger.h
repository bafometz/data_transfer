#ifndef LOGGER_H
#define LOGGER_H

#include <format>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string>
#include <utility>
#include <print>


namespace logger::internal {
    constexpr std::string_view red { "\033[1;31m" };
    constexpr std::string_view lightred { "\033[91m" };
    constexpr std::string_view green { "\033[1;32m" };
    constexpr std::string_view yellow { "\033[1;33m" };
    constexpr std::string_view cyan { "\033[0;36m" };
    constexpr std::string_view magenta { "\033[1;35m" };
    constexpr std::string_view blue { "\033[34m" };
    constexpr std::string_view lightBlue { "\033[94m" };
    constexpr std::string_view reset { "\033[0m" };
}
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

    template< int LogLvl, typename... Args >
    void log([[maybe_unused]] const std::string& sinkName, [[maybe_unused]] const std::source_location &location, std::string_view fmt, Args &&...args) {
        constexpr auto term_color = [] consteval -> auto  {
            if constexpr (LogLvl == 0)            return logger::internal::lightBlue;
            else if constexpr(LogLvl == 1)        return logger::internal::blue;
            else if constexpr (LogLvl == 2)       return logger::internal::green;
            else if constexpr (LogLvl == 3)       return logger::internal::yellow;
            else if constexpr (LogLvl == 4)       return logger::internal::lightred;
            else if constexpr (LogLvl == 5)       return logger::internal::red;
            else  static_assert((LogLvl < 0 || LogLvl > 5), "Color doesent exist");
        }();

        constexpr auto log_level = []consteval -> auto {
            if constexpr (LogLvl == 0) return "T";
            else if constexpr(LogLvl == 1) return "D";
            else if constexpr(LogLvl == 2) return "I";
            else if constexpr(LogLvl == 3) return "W";
            else if constexpr(LogLvl == 4) return "E";
            else if constexpr(LogLvl == 5) return "C";
            else  static_assert((LogLvl < 0 || LogLvl > 5), "Log level doesent exist");
        }();

        const auto data = std::vformat(fmt, std::make_format_args(args...));
        std::println("{}{}| {}{}", term_color, log_level, data, logger::internal::reset);
    }

    template< typename... Args >
    void log(int logLvl, [[maybe_unused]] const std::string& sinkName, [[maybe_unused]] const source_location &location,
             [[maybe_unused]] Args &&...args)
    {
        switch (logLvl)
        {
        case 0:
            {
                locker _(traceMutex);
                std::cout << logger::internal::lightBlue << "[TRACE] " << location << " " << location.funcname << " ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        case 1:
            {
                locker _(debugMutex);
                std::cout << logger::internal::blue << "[DEBUG] " /*<< location << " "*/;
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        case 2:
            {
                locker _(infoMutex);
                std::cout << logger::internal::green << "[INFO] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        case 3:
            {
                locker _(warnMutex);
                std::cout << logger::internal::yellow << "[WARN] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        case 4:
            {
                locker _(errorMutex);
                std::cout << logger::internal::lightred << "[ERROR] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        case 5:
            {
                locker _(criticalMutex);
                std::cout <<logger::internal::red << "[CRITICAL] ";
                ((std::cout << std::forward< Args >(args) << " "), ...);
                std::cout << logger::internal::reset << std::endl;
                return;
            }
        default:
            return;
        }
    }

  private:
    Logger() {};
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

// #define LOG_TRACE(...) loggerInstance.log(0, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
// #define LOG_DEBUG(...) loggerInstance.log(1, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)

#define OLD_LOG_INFO(...)     loggerInstance.log(2, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define OLD_LOG_WARN(...)     loggerInstance.log(3, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define OLD_LOG_ERROR(...)    loggerInstance.log(4, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)
#define OLD_LOG_CRITICAL(...) loggerInstance.log(5, "default", { __FILE__, __PRETTY_FUNCTION__, __LINE__ }, ##__VA_ARGS__)

#define LOG_TRACE(FMT,...)loggerInstance.log<0>("default", std::source_location{}, FMT, ##__VA_ARGS__)
#define LOG_DEBUG(FMT,...)loggerInstance.log<1>("default", std::source_location{},  FMT,##__VA_ARGS__)
#define LOG_INFO(FMT,...)loggerInstance.log<2>("default", std::source_location{},  FMT,##__VA_ARGS__)
#define LOG_WARN(FMT,...)loggerInstance.log<3>("default", std::source_location{},  FMT,##__VA_ARGS__)
#define LOG_ERROR(FMT,...)loggerInstance.log<4>("default", std::source_location{},  FMT,##__VA_ARGS__)
#define LOG_CRITICAL(FMT,...)loggerInstance.log<5>("default", std::source_location{},  FMT, ##__VA_ARGS__)

#endif  // LOGGER_H
