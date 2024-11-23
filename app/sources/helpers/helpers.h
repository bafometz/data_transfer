#ifndef HELPERS_H
#define HELPERS_H
#include <cstdint>
#include <string>

namespace helpers
{
    uint64_t    getFreeDiskSpace(const std::string& path);
    std::string pathToExec();
    bool        removeFile(const std::string&);

};  // namespace helpers

#endif  // HELPERS_H
