#include "include/archive.h"
#include <filesystem>
#include <cstdio>
#include <iostream>

namespace fs = std::filesystem;

bool extractArchive(const std::string& zipPath, const std::string& extractDir) {
    // Verify zip file exists
    if (!fs::exists(zipPath)) {
        std::cerr << "Archive does not exist: " << zipPath << std::endl;
        return false;
    }

    // Verify zip file is not empty
    try {
        if (fs::file_size(zipPath) == 0) {
            std::cerr << "Archive is empty: " << zipPath << std::endl;
            return false;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to check archive size: " << e.what() << std::endl;
        return false;
    }

    // Create extraction directory if it doesn't exist
    try {
        fs::create_directories(extractDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create extraction directory: " << e.what() << std::endl;
        return false;
    }

    // Build PowerShell command with better error handling
    std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"";
    command += "try { ";
    command += "Expand-Archive -LiteralPath '" + zipPath + "' ";
    command += "-DestinationPath '" + extractDir + "' -Force -ErrorAction Stop; ";
    command += "exit 0 ";
    command += "} catch { ";
    command += "Write-Error $_.Exception.Message; ";
    command += "exit 1 ";
    command += "}\"";

    std::cout << "Extracting archive: " << zipPath << std::endl;

    int result = system(command.c_str());

    if (result != 0) {
        std::cerr << "Extraction failed with exit code: " << result << std::endl;
        std::cerr << "The archive might be corrupted or incomplete." << std::endl;
        return false;
    }

    std::cout << "Extraction completed successfully." << std::endl;
    return true;
}