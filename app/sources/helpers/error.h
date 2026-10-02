#pragma once

#include <array>
#include <cerrno>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace data_transfer::errors
{
struct system_error
{
    system_error() noexcept
        : errno_ { errno } { };
    system_error(int err) noexcept
        : errno_(err) { };
    system_error(system_error &&err) noexcept
        : errno_ { 0 }
    {
        std::swap(err.errno_, errno_);
    };
    system_error &operator=(system_error &&err) noexcept
    {
        std::swap(err.errno_, errno_);
        return *this;
    };

    system_error(const system_error &) = delete;
    system_error &operator=(const system_error &) = delete;
    const int &value() const noexcept
    {
        return errno_;
    };

private:
    int errno_;
};

struct exception : std::runtime_error
{
    template <typename... Args>
    exception(const std::format_string<Args...> fmt, Args &&...args)
        : std::runtime_error { std::format(fmt, std::forward<Args>(args)...) } {};
};

inline std::string to_string(const system_error &error)
{
    std::array<char, 1024> buf {};
    return std::format("{} (errno = {})", strerror_r(error.value(), buf.data(), buf.size()), error.value());
}

};

template <> struct std::formatter<data_transfer::errors::system_error, char> : std::formatter<std::string_view, char>
{
    template <class FormatContext> auto format(const data_transfer::errors::system_error &value, FormatContext &ctx) const -> decltype(ctx.out())
    {
        return std::format_to(ctx.out(), "system error: {}(errcode={})", to_string(value), value.value());
    }
};