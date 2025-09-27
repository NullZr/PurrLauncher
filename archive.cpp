#include "include/archive.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

bool extractArchive(const std::string& zipPath, const std::string& extractDir) {
    fs::create_directories(extractDir);
    const std::string command = "powershell -command \"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + extractDir + "' -Force\"";
    return (system(command.c_str()) == 0);
}