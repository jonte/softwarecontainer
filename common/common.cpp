#include <string.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include "pelagicontain-common.h"

namespace pelagicontain {

bool fileHasMode(const std::string &path, int mode) {
    bool isDir = false;
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if ( (st.st_mode & mode) != 0 ) {
            isDir = true;
        }
    }
    return isDir;

}

bool isDirectory(const std::string &path) {
    return fileHasMode(path, S_IFDIR);
}

bool isSocket(const std::string &path) {
    return fileHasMode(path, S_IFSOCK);
}

std::string parentPath(const std::string &path) {
    static constexpr const char *separator = "/";
    auto pos = path.rfind(separator);
    if (pos == std::string::npos)
        pos = strlen(separator);
    std::string parentPath = path.substr(0, pos - strlen(separator) + 1);
    return parentPath;
}

ReturnCode touch(const std::string &path) {
    auto fd = open(path.c_str(), O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK | O_LARGEFILE, 0666);
    if (fd != -1) {
        close(fd);
        return ReturnCode::FAILURE;
    }
    return ReturnCode::SUCCESS;

}


}
