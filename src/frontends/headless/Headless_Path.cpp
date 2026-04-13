#include "core/Util_Path.h"
#include <unistd.h>
#include <limits.h>
#include <cstdlib>

namespace Path {

auto GetExecutableDir() -> std::string {
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        size_t pos = path.find_last_of('/');
        if (pos != std::string::npos) {
            return path.substr(0, pos + 1);
        }
    }
    return "./";
}

auto GetUserDataDir() -> std::string {
    const char* dataHome = getenv("XDG_DATA_HOME");
    if (dataHome) {
        return std::string(dataHome) + "/linapple/";
    }
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/share/linapple/";
    }
    return "./";
}

} // namespace Path
