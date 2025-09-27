// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// Main configuration functions
bool loadConfig(std::string& javaPath, std::string& username, std::string& uuid, bool& debug,
                std::string& max_ram, std::string& pack_url, std::string& pack_manifest_url,
                std::string& pack_version, std::string& log_file, std::string& api_url, std::string& auth_token);

void saveConfig(const std::string& javaPath, const std::string& username, const std::string& uuid,
                bool debug, const std::string& max_ram, const std::string& pack_url,
                const std::string& pack_manifest_url, const std::string& pack_version,
                const std::string& log_file, const std::string& api_url, const std::string& auth_token);

// Configuration validation and utility functions
bool isValidRamValue(const std::string& ramValue);
bool validateConfig(const std::string& javaPath, const std::string& max_ram,
                   const std::string& pack_url, const std::string& api_url);

// Configuration backup and restore functions
bool backupConfig();
bool restoreConfig();

#endif // CONFIG_H