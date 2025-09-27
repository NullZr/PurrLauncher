#ifndef MINECRAFT_H
#define MINECRAFT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Main functions
bool buildClasspathFromJson(const std::string& gameDir, const std::string& version);
void launchMinecraft(const std::string& javaPath, const std::string& username, const std::string& uuid,
                    const std::string& version, bool debug, const std::string& max_ram,
                    const std::string& gameDir, const std::string& log_file,
                    const std::string& accessToken, const std::string& userType, const std::string& api_url);
bool updatePack(const std::string& pack_url, const std::string& pack_manifest_url,
               std::string& pack_version, const std::string& gameDir,
               bool debug, const std::string& log_file);

// Library processing functions
bool processLibrary(const json& lib, const std::string& libDir,
                   std::vector<std::string>& classpathEntries, const std::string& gameDir);
bool isLibraryCompatible(const json& lib);
std::string getLibraryPath(const json& lib);
void processNatives(const json& lib, const std::string& gameDir);

// JSON and argument processing
bool loadVersionJson(const std::string& jsonPath, json& j, bool debug, const std::string& log_file);
bool loadClasspath(const std::string& gameDir, std::string& cp);
std::string getAssetIndexId(const json& j);
std::unordered_map<std::string, std::string> createPlaceholderMap(
    const std::string& username, const std::string& version, const std::string& gameDir,
    const std::string& assetIndexId, const std::string& uuid, const std::string& accessToken,
    const std::string& userType, const std::string& cp);

// Argument processing
std::string replacePlaceholders(const std::string& arg,
                               const std::unordered_map<std::string, std::string>& placeholders);
std::vector<std::string> processJvmArguments(const json& j,
    const std::unordered_map<std::string, std::string>& placeholders,
    const std::string& gameDir, const std::string& api_url,
    const std::string& accessToken, bool debug, const std::string& log_file);
std::vector<std::string> processGameArguments(const json& j,
    const std::unordered_map<std::string, std::string>& placeholders,
    const std::string& version, const std::string& gameDir, const std::string& assetIndexId,
    const std::string& uuid, const std::string& username, const std::string& accessToken,
    const std::string& userType);

// JVM argument helpers
void processModernJvmArgs(const json& jvmArgsJson, std::vector<std::string>& jvmArgs,
                         const std::unordered_map<std::string, std::string>& placeholders);
bool shouldIncludeConditionalArg(const json& arg);
void addConditionalArgs(const json& arg, std::vector<std::string>& jvmArgs,
                       const std::unordered_map<std::string, std::string>& placeholders);
void addAuthlibInjector(std::vector<std::string>& jvmArgs, const std::string& gameDir,
                       const std::string& api_url, const std::string& accessToken,
                       bool debug, const std::string& log_file);

// Game argument helpers
void processModernGameArgs(const json& gameArgsJson, std::vector<std::string>& gameArgs,
                          const std::unordered_map<std::string, std::string>& placeholders);

// Launch helpers
bool writeLaunchArgs(const std::string& gameDir, const std::string& max_ram,
                    const std::vector<std::string>& jvmArgs, const std::string& mainClass,
                    const std::vector<std::string>& gameArgs);
void executeLaunchCommand(const std::string& javaPath, const std::string& javawPath,
                         const std::string& gameDir, bool debug);

// Pack update helpers
bool cleanupDirectoriesForUpdate(const std::string& gameDir, bool debug, const std::string& log_file);
bool downloadAndExtractPack(const std::string& pack_url, const std::string& gameDir,
                           bool debug, const std::string& log_file);

// Utility functions
std::string replaceAll(std::string str, const std::string& from, const std::string& to);

#endif // MINECRAFT_H
