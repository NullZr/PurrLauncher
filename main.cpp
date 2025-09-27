#include "include/config.h"
#include "include/java.h"
#include "include/minecraft.h"
#include "include/crypto.h"
#include "include/logging.h"
#include "include/download.h"  // For httpGet and httpPost

#include <iostream>
#include <filesystem>
#include <cctype>  // For isalnum
#include <windows.h>  // For DLL loading (LoadLibrary, GetProcAddress, FreeLibrary)
#include <fstream>  // For reading launcher_version.txt
#include <sstream>
#include <thread>  // For std::this_thread::sleep_for
#include <iomanip>  // For std::setw, std::setfill
#include <memory>   // For smart pointers
#include <vector>
#include <string>
#include <io.h>     // For _open_osfhandle
#include <fcntl.h>  // For _O_TEXT
#include <limits>   // For std::numeric_limits

#include <nlohmann/json.hpp>  // Для парсинга JSON из API
#include <curl/curl.h>  // For curl_global_init

using json = nlohmann::json;
namespace fs = std::filesystem;

typedef void (*PluginInitFunc)();    // Type for the "Initialize" function in DLLs
typedef void (*PluginCleanupFunc)(); // Type for the "Cleanup" function in DLLs

// Declare function from plugin_downloader.cpp
extern void DownloadMissingPlugins(bool debug, const std::string& log_file);

// RAII wrapper for curl global initialization
class CurlManager {
public:
    CurlManager() { curl_global_init(CURL_GLOBAL_ALL); }
    ~CurlManager() { curl_global_cleanup(); }
    CurlManager(const CurlManager&) = delete;
    CurlManager& operator=(const CurlManager&) = delete;
};

// RAII wrapper for DLL modules
class PluginManager {
private:
    std::vector<HMODULE> loadedModules;

public:
    ~PluginManager() {
        for (HMODULE hModule : loadedModules) {
            if (auto cleanupFunc = reinterpret_cast<PluginCleanupFunc>(GetProcAddress(hModule, "Cleanup"))) {
                try {
                    cleanupFunc();
                } catch (...) {
                    // Log error but continue cleanup
                }
            }
            FreeLibrary(hModule);
        }
    }

    int loadPlugins(const std::string& pluginsDir, bool debug, const std::string& log_file) {
        log("Loading launcher plugins from " + pluginsDir + "...", debug, log_file);
        int loadedCount = 0;

        if (!fs::exists(pluginsDir)) {
            return 0;
        }

        for (const auto& entry : fs::directory_iterator(pluginsDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".dll") {
                continue;
            }

            const std::string dllPath = entry.path().string();
            HMODULE hModule = LoadLibraryA(dllPath.c_str());  // Use LoadLibraryA for char strings

            if (!hModule) {
                log("Failed to load plugin: " + dllPath, debug, log_file);
                continue;
            }

            auto initFunc = reinterpret_cast<PluginInitFunc>(GetProcAddress(hModule, "Initialize"));
            if (initFunc) {
                try {
                    initFunc();
                    log("Initialized plugin: " + dllPath, debug, log_file);
                } catch (...) {
                    log("Plugin initialization failed: " + dllPath, debug, log_file);
                    FreeLibrary(hModule);
                    continue;
                }
            }

            loadedModules.push_back(hModule);
            ++loadedCount;
        }

        log("Loaded " + std::to_string(loadedCount) + " launcher plugin(s).", debug, log_file);
        return loadedCount;
    }
};

// Configuration structure for better organization
struct LauncherConfig {
    std::string gameDir = "minecraft/";
    std::string version = "Forge 1.20.1";
    std::string javaPath;
    std::string username;
    std::string uuid;
    std::string max_ram = "6G";
    std::string pack_url = "https://your-api-server.com/modpack";
    std::string pack_manifest_url = "https://your-api-server.com/manifest";
    std::string pack_version;
    std::string log_file = "launcher.log";
    std::string api_url = "https://your-api-server.com";
    std::string auth_token;
    bool debug = false;
};

bool initializeConsole(const std::string& launcher_version) {
    SetConsoleOutputCP(CP_UTF8);

    const std::string title = "PurrLauncher version " + launcher_version + " written in C++ with <3";
    const std::wstring wtitle(title.begin(), title.end());

    if (!SetConsoleTitleW(wtitle.c_str())) {
        std::cout << "Failed to set console title. Error code: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}

std::string readLauncherVersion() {
    constexpr const char* version_file = "launcher_version.txt";
    std::ifstream vf(version_file);
    std::string launcher_version;

    if (vf && std::getline(vf, launcher_version)) {
        return launcher_version;
    }
    return "Unknown";
}

bool createDirectoryIfNotExists(const std::string& dir, bool debug, const std::string& log_file) {
    if (fs::exists(dir)) {
        return true;
    }

    try {
        fs::create_directories(dir);
        log("Created directory: " + dir, debug, log_file);
        return true;
    } catch (const fs::filesystem_error& e) {
        log("Failed to create directory " + dir + ": " + e.what(), debug, log_file);
        return false;
    }
}

bool authenticateUser(LauncherConfig& config, std::string& accessToken, std::string& userType) {
    // Generate HWID
    std::string hwid = getHWID();
    if (hwid.find("ERROR") == 0) {
        log("Failed to get HWID: " + hwid, config.debug, config.log_file);
        return false;
    }
    log("Generated HWID: " + hwid, config.debug, config.log_file);

    // Step 1: Validate token via custom API
    const std::string validateUrl = config.api_url + "/api/auth/validate?token=" +
                                   config.auth_token + "&hwid=" + hwid;
    log("Validating token via API: " + validateUrl, config.debug, config.log_file);

    const std::string validateResponse = httpGet(validateUrl);
    if (validateResponse.empty()) {
        log("Empty response from validate API. Check connection/URL.", config.debug, config.log_file);
        return false;
    }

    try {
        json validateJson = json::parse(validateResponse);
        if (!validateJson.contains("username")) {
            log("Invalid validate response: no username.", config.debug, config.log_file);
            return false;
        }

        config.username = validateJson["username"].get<std::string>();
        log("Authenticated username from API: " + config.username, config.debug, config.log_file);

        if (validateJson.contains("registered") && validateJson["registered"].get<bool>()) {
            log("HWID already registered.", config.debug, config.log_file);
        }
    } catch (const json::exception& e) {
        log("API validate parse error: " + std::string(e.what()), config.debug, config.log_file);
        return false;
    }

    // Step 2: Authenticate via Yggdrasil to get accessToken
    const std::string yggUrl = config.api_url + "/authserver/authenticate";
    json authPayload;
    authPayload["username"] = config.username;
    authPayload["password"] = config.auth_token;
    authPayload["clientToken"] = generateOfflineUUID(config.username);
    authPayload["requestUser"] = true;

    const std::string yggResponse = httpPost(yggUrl, authPayload.dump());
    if (!yggResponse.empty()) {
        try {
            json yggJson = json::parse(yggResponse);
            if (yggJson.contains("accessToken")) {
                accessToken = yggJson["accessToken"].get<std::string>();
                userType = "mojang";
                if (yggJson.contains("availableProfiles") && !yggJson["availableProfiles"].empty()) {
                    config.uuid = yggJson["availableProfiles"][0]["id"].get<std::string>();
                }
                log("Obtained accessToken from Yggdrasil: " + accessToken, config.debug, config.log_file);
                return true;
            } else {
                log("Yggdrasil response missing accessToken.", config.debug, config.log_file);
            }
        } catch (const json::exception& e) {
            log("Yggdrasil parse error: " + std::string(e.what()), config.debug, config.log_file);
        }
    } else {
        log("Empty or failed Yggdrasil response.", config.debug, config.log_file);
    }

    // Fallback to offline mode
    config.uuid = generateOfflineUUID(config.username);
    log("Falling back to offline mode with UUID: " + config.uuid, config.debug, config.log_file);
    accessToken = "0";
    userType = "legacy";
    return true;
}

int main() {
    // Ensure console window is available for input/output
    AllocConsole();

    // Redirect standard streams to console
    FILE* pCout;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    FILE* pCerr;
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    FILE* pCin;
    freopen_s(&pCin, "CONIN$", "r", stdin);

    // Synchronize C++ streams with C streams
    std::ios::sync_with_stdio(true);
    std::cin.tie(nullptr);

    // RAII management
    CurlManager curlManager;
    PluginManager pluginManager;

    // Read launcher version and initialize console
    const std::string launcher_version = readLauncherVersion();
    initializeConsole(launcher_version);

    // Initialize configuration
    LauncherConfig config;

    // Load configuration
    const bool javaLoaded = loadConfig(
        config.javaPath, config.username, config.uuid, config.debug,
        config.max_ram, config.pack_url, config.pack_manifest_url,
        config.pack_version, config.log_file, config.api_url, config.auth_token
    );

    // Check for token and prompt if needed
    if (config.auth_token.empty()) {
        log("No token found in config.", config.debug, config.log_file);

        // Force output to console
        std::cout << "Введите токен авторизации: ";
        std::cout.flush();

        // Clear stream state but don't ignore input
        std::cin.clear();

        std::string inputToken;
        std::getline(std::cin, inputToken);

        // Debug: log the raw input
        log("Raw input received: '" + inputToken + "' (length: " + std::to_string(inputToken.length()) + ")", config.debug, config.log_file);

        // Trim whitespace
        inputToken.erase(0, inputToken.find_first_not_of(" \t\r\n"));
        inputToken.erase(inputToken.find_last_not_of(" \t\r\n") + 1);

        // Debug: log the trimmed input
        log("Trimmed input: '" + inputToken + "' (length: " + std::to_string(inputToken.length()) + ")", config.debug, config.log_file);

        if (inputToken.empty()) {
            std::cout << "Токен не введен. Программа завершается." << std::endl;
            log("No token provided. Exiting.", config.debug, config.log_file);
            std::cout << "Нажмите Enter для выхода...";
            std::cin.get();
            return 1;
        }

        config.auth_token = inputToken;

        // Save token to config.json
        saveConfig(config.javaPath, config.username, config.uuid, config.debug, config.max_ram,
                   config.pack_url, config.pack_manifest_url, config.pack_version,
                   config.log_file, config.api_url, config.auth_token);
        log("Token saved to config file.", config.debug, config.log_file);
        std::cout << "Токен сохранен в конфигурацию." << std::endl;
    }

    // Create necessary directories
    if (!createDirectoryIfNotExists(config.gameDir, config.debug, config.log_file)) {
        return 1;
    }

    log("Starting PurrLauncher...", config.debug, config.log_file);

    // Create plugins directory and load plugins
    const std::string pluginsDir = "plugins/";
    createDirectoryIfNotExists(pluginsDir, config.debug, config.log_file);

#if defined(ENABLE_PLUGIN_DOWNLOAD)
    DownloadMissingPlugins(config.debug, config.log_file);
#endif

    // Load plugins using RAII manager
    pluginManager.loadPlugins(pluginsDir, config.debug, config.log_file);

    // Download and extract Java if not loaded
    if (!javaLoaded && !downloadAndExtractJava(config.javaPath)) {
        log("Failed to download/extract Java.", config.debug, config.log_file);
        return 1;
    }

    // Authenticate user
    std::string accessToken, userType;
    if (!authenticateUser(config, accessToken, userType)) {
        return 1;
    }

    // Create config directory
    const std::string configDir = config.gameDir + "config/";
    createDirectoryIfNotExists(configDir, config.debug, config.log_file);

    // Update pack if needed
    if (!updatePack(config.pack_url, config.pack_manifest_url, config.pack_version,
                   config.gameDir, config.debug, config.log_file)) {
        log("Failed to update pack.", config.debug, config.log_file);
        return 1;
    }

    // Create libraries directory and download authlib-injector
    const std::string librariesDir = config.gameDir + "libraries/";
    createDirectoryIfNotExists(librariesDir, config.debug, config.log_file);

    const std::string authlibPath = librariesDir + "authlib-injector.jar";
    if (!fs::exists(authlibPath)) {
        constexpr const char* authlibUrl = "https://authlib-injector.yushi.moe/artifact/53/authlib-injector-1.2.5.jar";
        log("Downloading authlib-injector from " + std::string(authlibUrl) + "...", config.debug, config.log_file);

        if (!downloadFile(authlibUrl, authlibPath)) {
            log("Failed to download authlib-injector.", config.debug, config.log_file);
            return 1;
        }
        log("Downloaded authlib-injector successfully.", config.debug, config.log_file);
    }

    // Save configuration
    saveConfig(config.javaPath, config.username, config.uuid, config.debug,
              config.max_ram, config.pack_url, config.pack_manifest_url,
              config.pack_version, config.log_file, config.api_url, config.auth_token);

    log("Pack updated to " + config.pack_version + ". Configuration saved.", config.debug, config.log_file);

    // Build classpath and launch Minecraft
    if (!buildClasspathFromJson(config.gameDir, config.version)) {
        log("Failed to build classpath.", config.debug, config.log_file);
        return 1;
    }

    // Launch Minecraft
    launchMinecraft(config.javaPath, config.username, config.uuid, config.version,
                   config.debug, config.max_ram, config.gameDir, config.log_file,
                   accessToken, userType, config.api_url);

    return 0;
}