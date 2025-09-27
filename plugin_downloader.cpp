#include "include/download.h"
#include "include/logging.h"
#include <filesystem>

namespace fs = std::filesystem;

void DownloadMissingPlugins(bool debug, const std::string& log_file) {
    const std::string pluginsDir = "plugins/";
    const std::string dllPath = pluginsDir + "updater_plugin.dll";
    const std::string dllUrl = "https://your-server.com/plugins/updater_plugin.dll";  // Replace with your plugin download URL

    if (!fs::exists(dllPath)) {
        log("Updater plugin DLL missing. Downloading from " + dllUrl + "...", debug, log_file);
        if (!downloadFile(dllUrl, dllPath)) {
            log("Failed to download updater plugin DLL.", debug, log_file);
            // Continue without plugin
        } else {
            log("Updater plugin DLL downloaded successfully.", debug, log_file);
        }
    }
}