#ifndef UTIL_PATH_H
#define UTIL_PATH_H

#include <string>
#include <vector>
#include <unistd.h>
#include <fstream>

#include <sys/stat.h>

namespace Path {

// Ensure directory exists (creates it recursively if it doesn't)
inline void EnsureDirExists(const std::string& path) {
    size_t pos = 0;
    do {
        pos = path.find_first_of('/', pos + 1);
        std::string subdir = path.substr(0, pos);
        if (!subdir.empty() && subdir != "/") {
            struct stat st;
            if (stat(subdir.c_str(), &st) != 0) {
                mkdir(subdir.c_str(), 0755);
            }
        }
    } while (pos != std::string::npos);
}

// Returns the directory where the executable is located.
auto GetExecutableDir() -> std::string;

// Returns the user's data directory (~/.local/share/linapple/)
auto GetUserDataDir() -> std::string;

// Returns the user's configuration directory (~/.config/linapple/)
inline auto GetUserConfigDir() -> std::string {
    const char* configHome = getenv("XDG_CONFIG_HOME");
    if (configHome) {
        return std::string(configHome) + "/linapple/";
    }
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/linapple/";
    }
    return GetUserDataDir();
}

// Returns a list of directories to search for data assets (ROMs, disks, config).
inline auto GetDataSearchPaths() -> std::vector<std::string> {
    std::vector<std::string> paths;

    paths.push_back(GetUserDataDir());
    paths.push_back(GetUserConfigDir());
    paths.push_back(GetExecutableDir());

    // Relocatable system installation paths
    paths.push_back(GetExecutableDir() + "../share/linapple/");
    paths.push_back(GetExecutableDir() + "../etc/linapple/");

#ifdef ASSET_DIR
    paths.push_back(ASSET_DIR "/");
#endif
#ifdef SYSCONF_DIR
    paths.push_back(SYSCONF_DIR "/");
#endif

    return paths;
}

// Attempts to find a file in the data search paths.
inline auto FindDataFile(const std::string& filename) -> std::string {
    for (const auto& dir : GetDataSearchPaths()) {
        std::string fullPath = dir + filename;
        if (access(fullPath.c_str(), R_OK) == 0) {
            return fullPath;
        }
    }
    return "";
}

// Standard binary file copy
inline auto CopyFile(const std::string& src, const std::string& dst) -> bool {
    std::ifstream src_file(src, std::ios::binary);
    if (!src_file.is_open()) return false;
    std::ofstream dst_file(dst, std::ios::binary);
    if (!dst_file.is_open()) return false;
    dst_file << src_file.rdbuf();
    return true;
}

} // namespace Path

#endif
