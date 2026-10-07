#include "time.h"
#include <iomanip>
#include <sstream>

std::string DateTime::getCurrentTimestampStr(const std::string &format, std::optional< ms_fmt > additional_ms) const
{
    using namespace std::chrono;
    const auto               creationTime = system_clock::now();
    const auto               ms           = duration_cast< milliseconds >(creationTime.time_since_epoch()) % 1000;
    const auto               timer        = system_clock::to_time_t(creationTime);
    std::tm            bt           = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, format.c_str());

    if (additional_ms != std::nullopt)
    {
        oss << additional_ms->c_str() << std::setfill('0') << std::setw(3) << ms.count();
    }

    return oss.str();
}

std::string DateTime::getTimestampStr(const uint64_t msSinceEpoch, const std::string &format, std::optional< ms_fmt > additional_ms) const
{
    using namespace std::chrono;
    const auto               ms    = duration_cast< milliseconds >(milliseconds(msSinceEpoch)) % 1000;
    const auto               timer = system_clock::to_time_t(system_clock::time_point(milliseconds(msSinceEpoch)));
    std::tm            bt    = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, format.c_str());

    if (additional_ms != std::nullopt)
    {
        oss << additional_ms->c_str() << std::setfill('0') << std::setw(3) << ms.count();
    }

    return oss.str();
}

uint64_t DateTime::getMsSinceEpoh() const
{
    using namespace std::chrono;
    return duration_cast< milliseconds >(std::chrono::system_clock::now().time_since_epoch()).count();
}

Timer::Timer(bool startAtCreation) :
    started_ { startAtCreation }
{
    if (started_) timepointStart_ = std::chrono::steady_clock::now();
}

void Timer::start()
{
    if (started_) return;
    timepointStart_ = std::chrono::steady_clock::now();
}

uint64_t Timer::stop()
{
    if (!started_) return 0;
    started_ = false;
    using namespace std::chrono;
    const auto stop       = steady_clock::now();
    const auto difference = stop - timepointStart_;
    return duration_cast< milliseconds >(difference).count();
}

uint64_t Timer::getLap()
{
    using namespace std::chrono;
    const auto stop       = steady_clock::now();
    const auto difference = stop - timepointStart_;
    return duration_cast< milliseconds >(difference).count();
}
