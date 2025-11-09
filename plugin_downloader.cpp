#include "include/download.h"
#include "include/logging.h"
#include <filesystem>

namespace fs = std::filesystem;

void DownloadMissingPlugins(const bool debug, const std::string& log_file) {
    const std::string pluginsDir = "plugins/";
    const std::string dllPath = pluginsDir + "updater_plugin.dll";
    const std::string dllUrl = "https://flurry.moe/update";  // URL для скачивания DLL (замените на реальный)

    if (!fs::exists(dllPath)) {
        log("Updater plugin DLL missing. Downloading from " + dllUrl + "...", debug, log_file);
        if (!downloadFile(dllUrl, dllPath)) {
            log("Failed to download updater plugin DLL.", debug, log_file);
            // Продолжаем без плагина
        } else {
            log("Updater plugin DLL downloaded successfully.", debug, log_file);
        }
    }
}