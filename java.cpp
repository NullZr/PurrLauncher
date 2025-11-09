#include "include/java.h"
#include "include/download.h"
#include "include/archive.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

bool downloadAndExtractJava(std::string& javaPath) {
    const std::string javaUrl = "https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.16%2B8/OpenJDK17U-jdk_x64_windows_hotspot_17.0.16_8.zip";
    const std::string zipPath = "jdk.zip";
    const std::string extractDir = "java17";
    const std::string innerDir = "jdk-17.0.16+8";

    std::cout << "Downloading Java 17 ZIP archive from " << javaUrl << "..." << std::endl;
    if (!downloadFile(javaUrl, zipPath)) return false;

    std::cout << "Extracting Java 17..." << std::endl;
    if (!extractArchive(zipPath, extractDir)) return false;

    javaPath = extractDir + "\\" + innerDir + "\\bin\\java.exe";
    std::string javawPath = extractDir + "\\" + innerDir + "\\bin\\javaw.exe";

    if (!fs::exists(javaPath) || !fs::exists(javawPath)) return false;

    fs::remove(zipPath);
    return true;
}