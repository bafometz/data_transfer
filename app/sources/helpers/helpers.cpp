#include "helpers.h"

#include <array>
#include <ftw.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{
    constexpr auto selfExecPath = "/proc/self/exe";
}

int removemFiles(const char *pathname, const struct stat *sbuf, int type, FTW *ftwb)
{
    if (remove(pathname) < 0)
    {
        return -1;
    }

    return 0;
}

uint64_t helpers::getFreeDiskSpace(const std::string &path)
{
    struct statvfs buf;
    if (-1 != statvfs(path.c_str(), &buf))
    {
        return (( int )((buf.f_bsize * ( double )buf.f_bavail)));
    }

    return 0;
}

std::string helpers::pathToExec()
{
    std::array< char, 256 > buff;
    ssize_t                 len = ::readlink(selfExecPath, buff.data(), buff.max_size());
    if (len != -1)
    {
        return std::string(buff.data());
    }

    return "";
}

bool helpers::removeFile(const std::string &path)
{
    return removemFiles(path.c_str(), NULL, 0, NULL) == 0;
}
