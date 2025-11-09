#include "include/config.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <string_view>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Configuration field mapping for type safety and easier maintenance
struct ConfigField {
    std::string jsonKey;
    std::function<void(const json&, const std::string&)> setter;
    std::function<void(json&, const std::string&)> getter;
};

// RAII wrapper for configuration file operations
class ConfigManager {
private:
    static constexpr const char* CONFIG_FILE = "config.json";
    json configData;

public:
    bool load() {
        if (!fs::exists(CONFIG_FILE)) {
            return false;
        }

        try {
            std::ifstream ifs(CONFIG_FILE);
            if (!ifs.is_open()) {
                std::cerr << "Failed to open config file for reading" << std::endl;
                return false;
            }

            ifs >> configData;
            return true;
        } catch (const json::exception& e) {
            std::cerr << "JSON parse error in config: " << e.what() << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            return false;
        }
    }

    bool save() const {
        try {
            std::ofstream ofs(CONFIG_FILE);
            if (!ofs.is_open()) {
                std::cerr << "Failed to open config file for writing" << std::endl;
                return false;
            }

            ofs << configData.dump(4);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error saving config: " << e.what() << std::endl;
            return false;
        }
    }

    template<typename T>
    T getValue(const std::string& key, const T& defaultValue) const {
        try {
            if (configData.contains(key) && !configData[key].is_null()) {
                return configData[key].get<T>();
            }
        } catch (const json::exception&) {
            // Return default value on parse error
        }
        return defaultValue;
    }

    template<typename T>
    void setValue(const std::string& key, const T& value) {
        configData[key] = value;
    }

    bool hasKey(const std::string& key) const {
        return configData.contains(key) && !configData[key].is_null();
    }
};

bool loadConfig(std::string& javaPath, std::string& username, std::string& uuid, bool& debug,
                std::string& max_ram, std::string& pack_url, std::string& pack_manifest_url,
                std::string& pack_version, std::string& log_file, std::string& api_url, std::string& auth_token) {

    // Initialize with defaults
    debug = true;
    max_ram = "6G";
    pack_url = "";
    pack_manifest_url = "";
    pack_version = "0.0.0";
    log_file = "launcher.log";
    api_url = "https://your-api-server.com";
    auth_token = "";

    ConfigManager config;
    if (!config.load()) {
        return false;
    }

    bool javaLoaded = false;

    // Load Java configuration with validation
    if (config.getValue<bool>("java_downloaded", false)) {
        std::string tempJavaPath = config.getValue<std::string>("java_path", "");
        if (!tempJavaPath.empty() && fs::exists(tempJavaPath)) {
            javaPath = std::move(tempJavaPath);
            javaLoaded = true;
        }
    }

    // Load all configuration values efficiently
    username = config.getValue<std::string>("username", "");
    uuid = config.getValue<std::string>("uuid", "");
    debug = config.getValue<bool>("debug", true);

    // Load optional configuration with validation
    std::string tempMaxRam = config.getValue<std::string>("max_ram", "4G");
    if (isValidRamValue(tempMaxRam)) {
        max_ram = std::move(tempMaxRam);
    }

    pack_url = config.getValue<std::string>("pack_url", "");
    pack_manifest_url = config.getValue<std::string>("pack_manifest_url", "");
    pack_version = config.getValue<std::string>("pack_version", "0.0.0");
    log_file = config.getValue<std::string>("log_file", "launcher.log");
    api_url = config.getValue<std::string>("api_url", "https://your-api-server.com");
    auth_token = config.getValue<std::string>("auth_token", "");

    return javaLoaded;
}

void saveConfig(const std::string& javaPath, const std::string& username, const std::string& uuid,
                bool debug, const std::string& max_ram, const std::string& pack_url,
                const std::string& pack_manifest_url, const std::string& pack_version,
                const std::string& log_file, const std::string& api_url, const std::string& auth_token) {

    ConfigManager config;
    config.load(); // Load existing config to preserve other settings

    // Save Java configuration
    if (!javaPath.empty() && fs::exists(javaPath)) {
        config.setValue("java_downloaded", true);
        config.setValue("java_path", javaPath);
    }

    // Save user configuration
    if (!username.empty()) {
        config.setValue("username", username);
        config.setValue("uuid", uuid);
    }

    // Always save debug setting
    config.setValue("debug", debug);

    // Save optional settings only if they have values
    if (!max_ram.empty() && isValidRamValue(max_ram)) {
        config.setValue("max_ram", max_ram);
    }

    // Always save pack configuration with defaults if empty
    config.setValue("pack_url", pack_url.empty() ? "https://your-api-server.com/pack" : pack_url);
    config.setValue("pack_manifest_url", pack_manifest_url.empty() ? "https://your-api-server.com/manifest" : pack_manifest_url);
    config.setValue("pack_version", pack_version);

    if (!log_file.empty()) {
        config.setValue("log_file", log_file);
    }

    if (!api_url.empty()) {
        config.setValue("api_url", api_url);
    }

    if (!auth_token.empty()) {
        config.setValue("auth_token", auth_token);
    }

    if (!config.save()) {
        std::cerr << "Failed to save configuration!" << std::endl;
    }
}

// Utility function to validate RAM values
bool isValidRamValue(const std::string& ramValue) {
    if (ramValue.empty()) return false;

    // Check if it ends with G or M
    char unit = ramValue.back();
    if (unit != 'G' && unit != 'M' && unit != 'g' && unit != 'm') {
        return false;
    }

    // Check if the numeric part is valid
    std::string numericPart = ramValue.substr(0, ramValue.length() - 1);
    if (numericPart.empty()) return false;

    try {
        int value = std::stoi(numericPart);

        // Validate reasonable ranges
        if (unit == 'G' || unit == 'g') {
            return value >= 1 && value <= 32; // 1GB to 32GB
        } else {
            return value >= 512 && value <= 32768; // 512MB to 32GB in MB
        }
    } catch (const std::exception&) {
        return false;
    }
}

// Enhanced configuration validation
bool validateConfig(const std::string& javaPath, const std::string& max_ram,
                   const std::string& pack_url, const std::string& api_url) {
    bool isValid = true;

    // Validate Java path
    if (!javaPath.empty() && !fs::exists(javaPath)) {
        std::cerr << "Warning: Java path does not exist: " << javaPath << std::endl;
        isValid = false;
    }

    // Validate RAM setting
    if (!max_ram.empty() && !isValidRamValue(max_ram)) {
        std::cerr << "Warning: Invalid RAM value: " << max_ram << std::endl;
        isValid = false;
    }

    // Validate URLs (basic check)
    if (!pack_url.empty() && pack_url.find("http") != 0) {
        std::cerr << "Warning: Pack URL should start with http/https: " << pack_url << std::endl;
    }

    if (!api_url.empty() && api_url.find("http") != 0) {
        std::cerr << "Warning: API URL should start with http/https: " << api_url << std::endl;
    }

    return isValid;
}

// Backup configuration
bool backupConfig() {
    constexpr const char* CONFIG_FILE = "config.json";
    const std::string backupFile = "config.json.bak";

    if (!fs::exists(CONFIG_FILE)) {
        return true; // No config to backup
    }

    try {
        fs::copy_file(CONFIG_FILE, backupFile, fs::copy_options::overwrite_existing);
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to backup config: " << e.what() << std::endl;
        return false;
    }
}

// Restore configuration from backup
bool restoreConfig() {
    const std::string backupFile = "config.json.bak";
    constexpr const char* CONFIG_FILE = "config.json";

    if (!fs::exists(backupFile)) {
        return false;
    }

    try {
        fs::copy_file(backupFile, CONFIG_FILE, fs::copy_options::overwrite_existing);
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to restore config: " << e.what() << std::endl;
        return false;
    }
}