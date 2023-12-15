#ifndef HELPERS_H
#define HELPERS_H
#include <cstdint>
#include <string>

namespace helpers
{
    uint64_t    fileSize(const std::string& path);
    uint64_t    getFreeDiskSpace(const std::string& path);
    bool        isFileExist(const std::string& fileName);
    std::string pathToExec();
    std::string getDir(const std::string& pathToFile);
    bool        removeFile(const std::string&);

};  // namespace helpers

#endif  // HELPERS_H
