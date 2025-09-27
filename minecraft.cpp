#include <windows.h>
#include <wincrypt.h>
#include "include/minecraft.h"
#include "include/download.h"
#include "include/archive.h"
#include "include/logging.h"
#include "include/crypto.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <cstdio>
#include <string>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <sstream>
#include <memory>
#include <algorithm>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Optimized string replacement with better memory management
std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;

    size_t start_pos = 0;
    const size_t from_len = from.length();
    const size_t to_len = to.length();

    // Reserve space to reduce reallocations
    if (to_len > from_len) {
        str.reserve(str.size() + (to_len - from_len) * 5); // Estimate 5 replacements
    }

    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from_len, to);
        start_pos += to_len;
    }
    return str;
}

// RAII wrapper for file operations
class FileManager {
private:
    std::string filepath;
    std::ofstream file;

public:
    FileManager(const std::string& path) : filepath(path), file(path) {}

    ~FileManager() {
        if (file.is_open()) {
            file.close();
        }
    }

    bool isOpen() const { return file.is_open(); }

    template<typename T>
    FileManager& operator<<(const T& data) {
        file << data;
        return *this;
    }

    void flush() { file.flush(); }
};

// Optimized classpath building with better error handling
bool buildClasspathFromJson(const std::string& gameDir, const std::string& version) {
    const std::string jsonPath = gameDir + "versions/" + version + "/" + version + ".json";

    if (!fs::exists(jsonPath)) {
        std::cerr << "Version JSON not found: " << jsonPath << std::endl;
        return false;
    }

    json j;
    try {
        std::ifstream ifs(jsonPath);
        if (!ifs.is_open()) {
            std::cerr << "Failed to open: " << jsonPath << std::endl;
            return false;
        }
        ifs >> j;
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "File read error: " << e.what() << std::endl;
        return false;
    }

    std::vector<std::string> classpathEntries;
    classpathEntries.reserve(100); // Reserve space for typical number of libraries

    const std::string libDir = gameDir + "libraries/";

    if (j.contains("libraries") && j["libraries"].is_array()) {
        for (const auto& lib : j["libraries"]) {
            if (!processLibrary(lib, libDir, classpathEntries, gameDir)) {
                continue; // Skip problematic libraries but continue processing
            }
        }
    }

    // Add client JAR
    const std::string clientPath = gameDir + "versions/" + version + "/" + version + ".jar";
    if (fs::exists(clientPath)) {
        classpathEntries.push_back(clientPath);
    } else {
        std::cerr << "Missing client JAR: " << clientPath << std::endl;
        return false;
    }

    // Build classpath string more efficiently
    std::string cp;
    if (!classpathEntries.empty()) {
        cp.reserve(classpathEntries.size() * 100); // Estimate average path length

        cp = classpathEntries[0];
        for (size_t i = 1; i < classpathEntries.size(); ++i) {
            cp += ";" + classpathEntries[i];
        }
    }

    // Use RAII file manager
    try {
        FileManager cpFile(gameDir + "classpath.txt");
        if (!cpFile.isOpen()) {
            std::cerr << "Failed to create classpath.txt" << std::endl;
            return false;
        }
        cpFile << cp;
        cpFile.flush();
    } catch (const std::exception& e) {
        std::cerr << "Failed to write classpath: " << e.what() << std::endl;
        return false;
    }

    return true;
}

// Helper function to process individual library entries
bool processLibrary(const json& lib, const std::string& libDir,
                   std::vector<std::string>& classpathEntries, const std::string& gameDir) {
    // Check library rules for OS compatibility
    if (!isLibraryCompatible(lib)) {
        return false;
    }

    // Handle artifact downloads
    if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
        bool downloadOnly = lib.contains("downloadOnly") && lib["downloadOnly"].get<bool>();
        if (!downloadOnly) {
            std::string path = getLibraryPath(lib);
            if (!path.empty()) {
                const std::string localPath = libDir + path;
                if (fs::exists(localPath)) {
                    classpathEntries.push_back(localPath);
                } else {
                    std::cerr << "Missing library: " << localPath << std::endl;
                }
            }
        }
    }

    // Handle natives
    processNatives(lib, gameDir);
    return true;
}

// Check if library is compatible with current OS
bool isLibraryCompatible(const json& lib) {
    if (!lib.contains("rules")) {
        return true;
    }

    bool include = true;
    for (const auto& rule : lib["rules"]) {
        if (!rule.contains("action")) continue;

        const std::string action = rule["action"];
        bool osMatch = true;

        if (rule.contains("os") && rule["os"].contains("name")) {
            const std::string osName = rule["os"]["name"];
            osMatch = (osName == "windows");
        }

        if (action == "allow" && !osMatch) include = false;
        if (action == "disallow" && osMatch) include = false;
    }

    return include;
}

// Extract library path from JSON
std::string getLibraryPath(const json& lib) {
    if (lib["downloads"]["artifact"].contains("path") &&
        lib["downloads"]["artifact"]["path"].is_string()) {
        return lib["downloads"]["artifact"]["path"].get<std::string>();
    }

    // Construct path from name
    if (!lib.contains("name") || !lib["name"].is_string()) {
        return "";
    }

    const std::string name = lib["name"].get<std::string>();
    const size_t colon1 = name.find(':');
    const size_t colon2 = name.find(':', colon1 + 1);
    const size_t colon3 = name.find(':', colon2 + 1);

    if (colon1 == std::string::npos || colon2 == std::string::npos) {
        return "";
    }

    const std::string group = name.substr(0, colon1);
    const std::string artifact = name.substr(colon1 + 1, colon2 - colon1 - 1);
    const std::string ver = name.substr(colon2 + 1,
        colon3 != std::string::npos ? colon3 - colon2 - 1 : name.length() - colon2 - 1);
    const std::string classifier = colon3 != std::string::npos ? name.substr(colon3 + 1) : "";

    return replaceAll(group, ".", "/") + '/' + artifact + '/' + ver + '/' +
           artifact + '-' + ver + (classifier.empty() ? "" : "-" + classifier) + ".jar";
}

// Process native libraries
void processNatives(const json& lib, const std::string& gameDir) {
    if (!lib.contains("natives") || !lib["natives"].contains("windows")) {
        return;
    }

    const std::string classifier = lib["natives"]["windows"];
    if (!lib["downloads"].contains("classifiers") ||
        !lib["downloads"]["classifiers"].contains(classifier)) {
        return;
    }

    const std::string url = lib["downloads"]["classifiers"][classifier]["url"];
    const std::string tempJar = gameDir + "temp_natives.jar";
    const std::string nativesDir = gameDir + "natives/";

    if (!fs::exists(nativesDir) || fs::is_empty(nativesDir)) {
        std::cout << "Downloading natives from " << url << "..." << std::endl;
        if (downloadFile(url, tempJar)) {
            std::cout << "Extracting natives..." << std::endl;
            if (extractArchive(tempJar, nativesDir)) {
                fs::remove(tempJar);
            }
        }
    }
}

void launchMinecraft(const std::string& javaPath, const std::string& username, const std::string& uuid,
                    const std::string& version, bool debug, const std::string& max_ram,
                    const std::string& gameDir, const std::string& log_file,
                    const std::string& accessToken, const std::string& userType, const std::string& api_url) {

    log("Starting Minecraft launch process.", debug, log_file);

    // Use more efficient path operations
    const fs::path javaPathObj(javaPath);
    const std::string javawPath = (javaPathObj.parent_path() / "javaw.exe").string();
    log("javaw path: " + javawPath, debug, log_file);

    // Load and parse version JSON
    const std::string jsonPath = gameDir + "versions/" + version + "/" + version + ".json";
    json j;
    if (!loadVersionJson(jsonPath, j, debug, log_file)) {
        return;
    }

    // Get main class with fallback
    std::string mainClass = "cpw.mods.bootstraplauncher.BootstrapLauncher";
    if (j.contains("mainClass") && j["mainClass"].is_string() && !j["mainClass"].is_null()) {
        mainClass = j["mainClass"].get<std::string>();
    }
    log("Main class: " + mainClass, debug, log_file);

    // Load classpath efficiently
    std::string cp;
    if (!loadClasspath(gameDir, cp)) {
        log("Failed to load classpath", debug, log_file);
        return;
    }

    // Get asset index
    const std::string assetIndexId = getAssetIndexId(j);
    log("Asset index ID: " + assetIndexId, debug, log_file);

    // Create placeholder map for argument substitution
    const auto placeholders = createPlaceholderMap(username, version, gameDir, assetIndexId,
                                                  uuid, accessToken, userType, cp);

    // Process JVM arguments
    std::vector<std::string> jvmArgs = processJvmArguments(j, placeholders, gameDir,
                                                          api_url, accessToken, debug, log_file);

    // Process game arguments
    std::vector<std::string> gameArgs = processGameArguments(j, placeholders, version,
                                                            gameDir, assetIndexId, uuid,
                                                            username, accessToken, userType);

    // Write launch arguments to file
    if (!writeLaunchArgs(gameDir, max_ram, jvmArgs, mainClass, gameArgs)) {
        log("Failed to write launch arguments", debug, log_file);
        return;
    }

    // Execute launch command
    executeLaunchCommand(javaPath, javawPath, gameDir, debug);
}

// Helper function to load version JSON
bool loadVersionJson(const std::string& jsonPath, json& j, bool debug, const std::string& log_file) {
    if (!fs::exists(jsonPath)) {
        log("Version JSON not found: " + jsonPath, debug, log_file);
        return false;
    }

    try {
        std::ifstream ifs(jsonPath);
        if (!ifs.is_open()) {
            log("Failed to open version JSON: " + jsonPath, debug, log_file);
            return false;
        }
        ifs >> j;
        return true;
    } catch (const json::exception& e) {
        log("JSON parse error: " + std::string(e.what()), debug, log_file);
        return false;
    }
}

// Helper function to load classpath
bool loadClasspath(const std::string& gameDir, std::string& cp) {
    const std::string cpPath = gameDir + "classpath.txt";
    std::ifstream cpIfs(cpPath);
    if (!cpIfs.is_open()) {
        return false;
    }

    cp.assign(std::istreambuf_iterator<char>(cpIfs), std::istreambuf_iterator<char>());
    return true;
}

// Get asset index ID from JSON
std::string getAssetIndexId(const json& j) {
    if (j.contains("assets") && j["assets"].is_string() && !j["assets"].is_null()) {
        return j["assets"].get<std::string>();
    }
    if (j.contains("assetIndex") && j["assetIndex"].is_object() &&
        j["assetIndex"].contains("id") && j["assetIndex"]["id"].is_string() &&
        !j["assetIndex"]["id"].is_null()) {
        return j["assetIndex"]["id"].get<std::string>();
    }
    return "5"; // fallback
}

// Create placeholder map for argument substitution
std::unordered_map<std::string, std::string> createPlaceholderMap(
    const std::string& username, const std::string& version, const std::string& gameDir,
    const std::string& assetIndexId, const std::string& uuid, const std::string& accessToken,
    const std::string& userType, const std::string& cp) {

    return {
        {"auth_player_name", username},
        {"version_name", version},
        {"game_directory", gameDir},
        {"assets_root", gameDir + "assets"},
        {"assets_index_name", assetIndexId},
        {"auth_uuid", uuid},
        {"auth_access_token", accessToken},
        {"user_type", userType},
        {"version_type", "release"},
        {"resolution_width", "854"},
        {"resolution_height", "480"},
        {"classpath", cp},
        {"natives_directory", gameDir + "natives"},
        {"launcher_name", "PurrLauncher"},
        {"launcher_version", "2.4.104"},
        {"clientid", ""},
        {"auth_xuid", ""},
        {"quickPlayPath", ""},
        {"quickPlaySingleplayer", ""},
        {"quickPlayMultiplayer", ""},
        {"quickPlayRealms", ""},
        {"fml.forgeVersion", "47.4.6"},
        {"fml.mcVersion", "1.20.1"},
        {"fml.forgeGroup", "net.minecraftforge"},
        {"fml.mcpVersion", "20230612.114412"},
        {"library_directory", gameDir + "libraries"},
        {"classpath_separator", ";"}
    };
}

// Optimized placeholder replacement
std::string replacePlaceholders(const std::string& arg,
                               const std::unordered_map<std::string, std::string>& placeholders) {
    std::string result = arg;
    size_t pos = 0;

    while ((pos = result.find("${", pos)) != std::string::npos) {
        const size_t end = result.find('}', pos);
        if (end == std::string::npos) break;

        const std::string key = result.substr(pos + 2, end - pos - 2);
        const auto it = placeholders.find(key);
        if (it != placeholders.end()) {
            result.replace(pos, end - pos + 1, it->second);
            pos += it->second.length();
        } else {
            pos = end + 1;
        }
    }

    return result;
}

// Process JVM arguments with optimizations
std::vector<std::string> processJvmArguments(const json& j,
    const std::unordered_map<std::string, std::string>& placeholders,
    const std::string& gameDir, const std::string& api_url,
    const std::string& accessToken, bool debug, const std::string& log_file) {

    std::vector<std::string> jvmArgs;
    jvmArgs.reserve(20); // Reserve space for typical JVM args

    if (j.contains("arguments") && j["arguments"].is_object() &&
        j["arguments"].contains("jvm") && j["arguments"]["jvm"].is_array()) {

        processModernJvmArgs(j["arguments"]["jvm"], jvmArgs, placeholders);
    } else {
        // Legacy JVM args
        jvmArgs.push_back("-Djava.library.path=" + gameDir + "natives");
        jvmArgs.emplace_back("-cp");
        jvmArgs.push_back(placeholders.at("classpath"));
    }

    // Add authlib-injector support
    addAuthlibInjector(jvmArgs, gameDir, api_url, accessToken, debug, log_file);

    return jvmArgs;
}

// Process modern JVM arguments format
void processModernJvmArgs(const json& jvmArgsJson, std::vector<std::string>& jvmArgs,
                         const std::unordered_map<std::string, std::string>& placeholders) {
    for (const auto& arg : jvmArgsJson) {
        if (arg.is_null()) continue;

        if (arg.is_string()) {
            jvmArgs.push_back(replacePlaceholders(arg.get<std::string>(), placeholders));
        } else if (arg.is_object() && shouldIncludeConditionalArg(arg)) {
            addConditionalArgs(arg, jvmArgs, placeholders);
        }
    }
}

// Check if conditional argument should be included
bool shouldIncludeConditionalArg(const json& arg) {
    if (!arg.contains("rules") || !arg["rules"].is_array()) {
        return true;
    }

    bool includeArg = true;
    for (const auto& rule : arg["rules"]) {
        if (!rule.contains("action") || !rule["action"].is_string()) continue;

        const std::string action = rule["action"].get<std::string>();
        bool osMatch = true;

        if (rule.contains("os") && rule["os"].is_object() &&
            rule["os"].contains("name") && rule["os"]["name"].is_string()) {
            const std::string osName = rule["os"]["name"].get<std::string>();
            osMatch = (osName == "windows");
        }

        if (action == "allow" && !osMatch) includeArg = false;
        if (action == "disallow" && osMatch) includeArg = false;
    }

    return includeArg;
}

// Add conditional arguments to JVM args
void addConditionalArgs(const json& arg, std::vector<std::string>& jvmArgs,
                       const std::unordered_map<std::string, std::string>& placeholders) {
    if (!arg.contains("value")) return;

    if (arg["value"].is_string()) {
        jvmArgs.push_back(replacePlaceholders(arg["value"].get<std::string>(), placeholders));
    } else if (arg["value"].is_array()) {
        for (const auto& val : arg["value"]) {
            if (val.is_string()) {
                jvmArgs.push_back(replacePlaceholders(val.get<std::string>(), placeholders));
            }
        }
    }
}

// Add authlib-injector support
void addAuthlibInjector(std::vector<std::string>& jvmArgs, const std::string& gameDir,
                       const std::string& api_url, const std::string& accessToken,
                       bool debug, const std::string& log_file) {
    const std::string authlibPath = gameDir + "libraries/authlib-injector.jar";

    if (accessToken != "0" && !accessToken.empty() && fs::exists(authlibPath)) {
        const std::string agentArg = "-javaagent:" + authlibPath + "=" + api_url;
        constexpr const char* CertAgent = "-Dauthlibinjector.yggdrasil.prefetched=ewogICJzaWduYXR1cmVQdWJsaWNrZXkiOiAiLS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS1cbk1JSUJJakFOQmdrcWhraUc5dzBCQVFFRkFBT0NBUThBTUlJQkNnS0NBUUVBendPSEZpUy9rQzlickZONm5qT2laVytJS0U5ZEEyd2hcbk03SXo2QzRNWEFiNk1XKzdqSks1UnFuS290ekM1a3M4TkFXSGc0dGhKMjNNbU0zVVU2amVHdEt4Vy9JZVMrRjFzeEt6ZDFHNnJ2SUtcbnlJNGhkL2dWdDJOWGdlT0hQVFNRV0t2emEwUXM5REcrUHpNSU56VEJ2KzE1WHJxaDBsblI3Y2xjVXh6T0p5TXBpRXdmdTNHdnBLSktcbmhzUGsvVlBrK2lVMjJhZjVZSy93eDNZTS9mVklZM2ZvMlNmTGZ0UzVZbWJnT0pyenRJTzdYbFdWRDhHeWdqUC9kamxJT04vajBLbXhcbk5LaDIwenpiaHozNGk3azVlclo3UTlhelZGeHlWZWZsaGtGc0NiMXZuM2FWYzBwUGdiOVpkVzMzd25POFJtRmIzODQxWkJhQTZadmFcbnQxWG1wUUlEQVFBQlxuLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tXG4iLAogICJza2luRG9tYWlucyI6IFsKICAgICJmbHVycnkubW9lIiwKICAgICIuZmx1cnJ5Lm1vZSIKICBdLAogICJtZXRhIjogewogICAgInNlcnZlck5hbWUiOiAiRmx1cnJ5IEF1dGggU2VydmVyIiwKICAgICJpbXBsZW1lbnRhdGlvbk5hbWUiOiAiSmF2YSIsCiAgICAiaW1wbGVtZW50YXRpb25WZXJzaW9uIjogIjEuMCIsCiAgICAibGlua3MiOiB7CiAgICAgICJob21lcGFnZSI6ICJodHRwczovL2ZsdXJyeS5tb2UiLAogICAgICAicmVnaXN0ZXIiOiAiaHR0cHM6Ly9mbHVycnkubW9lL3JlZ2lzdGVyIgogICAgfQogIH0sCiAgImZlYXR1cmVzIjogewogICAgIm5vbl9lbWFpbF9sb2dpbiI6IHRydWUsCiAgICAiZW5hYmxlX3Byb2ZpbGVfa2V5IjogdHJ1ZSwKICAgICJmZWF0dXJlLm5vX21vamFuZ19uYW1lc3BhY2UiOiB0cnVlCiAgfQp9";

        jvmArgs.insert(jvmArgs.begin(), {CertAgent, agentArg});
        log("Added authlib-injector for online mode with server " + api_url, debug, log_file);
    } else {
        log("Offline mode detected or authlib-injector missing. Skipping authlib-injector.", debug, log_file);
    }
}

// Process game arguments
std::vector<std::string> processGameArguments(const json& j,
    const std::unordered_map<std::string, std::string>& placeholders,
    const std::string& version, const std::string& gameDir, const std::string& assetIndexId,
    const std::string& uuid, const std::string& username, const std::string& accessToken,
    const std::string& userType) {

    std::vector<std::string> gameArgs;
    gameArgs.reserve(20);

    if (j.contains("arguments") && j["arguments"].is_object() &&
        j["arguments"].contains("game") && j["arguments"]["game"].is_array()) {

        processModernGameArgs(j["arguments"]["game"], gameArgs, placeholders);
    } else {
        // Legacy game args
        gameArgs = {
            "--version", version,
            "--gameDir", gameDir,
            "--assetsDir", gameDir + "assets",
            "--assetIndex", assetIndexId,
            "--uuid", uuid,
            "--username", username,
            "--accessToken", accessToken,
            "--userType", userType
        };
    }

    return gameArgs;
}

// Process modern game arguments format
void processModernGameArgs(const json& gameArgsJson, std::vector<std::string>& gameArgs,
                          const std::unordered_map<std::string, std::string>& placeholders) {
    for (const auto& arg : gameArgsJson) {
        if (arg.is_null()) continue;

        if (arg.is_string()) {
            gameArgs.push_back(replacePlaceholders(arg.get<std::string>(), placeholders));
        } else if (arg.is_object()) {
            // Skip feature-dependent arguments for now
            bool includeArg = true;
            if (arg.contains("rules") && arg["rules"].is_array()) {
                for (const auto& rule : arg["rules"]) {
                    if (rule.contains("action") && rule["action"].is_string()) {
                        const std::string action = rule["action"].get<std::string>();
                        if (rule.contains("features")) {
                            // Skip optional features like demo, quickplay
                            includeArg = false;
                            break;
                        }
                    }
                }
            }

            if (includeArg && arg.contains("value")) {
                addConditionalArgs(arg, gameArgs, placeholders);
            }
        }
    }
}

// Write launch arguments to file
bool writeLaunchArgs(const std::string& gameDir, const std::string& max_ram,
                    const std::vector<std::string>& jvmArgs, const std::string& mainClass,
                    const std::vector<std::string>& gameArgs) {
    try {
        FileManager argFile(gameDir + "launch_args.txt");
        if (!argFile.isOpen()) {
            return false;
        }

        auto writeArg = [&argFile](const std::string& arg) {
            std::string out = arg;
            if (out.find(' ') != std::string::npos) {
                out = "\"" + out + "\"";
            }
            argFile << out << "\n";
        };

        if (!max_ram.empty()) {
            writeArg("-Xmx" + max_ram);
        }

        for (const auto& arg : jvmArgs) {
            writeArg(arg);
        }

        writeArg(mainClass);

        for (const auto& arg : gameArgs) {
            writeArg(arg);
        }

        argFile.flush();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write launch args: " << e.what() << std::endl;
        return false;
    }
}

// Execute the launch command
void executeLaunchCommand(const std::string& javaPath, const std::string& javawPath,
                         const std::string& gameDir, bool debug) {
    const std::string javaExec = debug ? javaPath : javawPath;
    std::string command;

    if (debug) {
        std::cout << "Launching in debug mode (console output enabled)..." << std::endl;
        command = "\"" + javaExec + "\" @" + gameDir + "launch_args.txt";
    } else {
        command = R"(start "" ")" + javaExec + "\" @" + gameDir + "launch_args.txt";
    }

    system(command.c_str());
}

bool updatePack(const std::string& pack_url, const std::string& pack_manifest_url,
               std::string& pack_version, const std::string& gameDir,
               bool debug, const std::string& log_file) {

    if (pack_url.empty() || pack_manifest_url.empty()) {
        log("No pack URL or manifest URL specified in config. Skipping update.", debug, log_file);
        return true;
    }

    // Download remote manifest with better error handling
    const std::string temp_manifest_path = gameDir + "remote_manifest.json";
    log("Downloading remote manifest from " + pack_manifest_url + "...", debug, log_file);

    if (!downloadFile(pack_manifest_url, temp_manifest_path)) {
        log("Failed to fetch remote manifest.", debug, log_file);
        return false;
    }

    // Parse remote version with RAII
    std::string remote_version;
    {
        std::ifstream manifest_ifs(temp_manifest_path);
        if (!manifest_ifs.is_open()) {
            log("Failed to open manifest file", debug, log_file);
            fs::remove(temp_manifest_path);
            return false;
        }

        try {
            json manifest_j;
            manifest_ifs >> manifest_j;
            remote_version = manifest_j.value("version", "0.0.0");
        } catch (const json::exception& e) {
            log("JSON parse error in manifest: " + std::string(e.what()), debug, log_file);
            fs::remove(temp_manifest_path);
            return false;
        }
    } // manifest_ifs automatically closed here

    if (remote_version == pack_version) {
        log("Pack is up to date (" + pack_version + ").", debug, log_file);
        fs::remove(temp_manifest_path);
        return true;
    }

    // Clean up directories more efficiently
    if (!cleanupDirectoriesForUpdate(gameDir, debug, log_file)) {
        log("Failed to cleanup directories for update", debug, log_file);
        fs::remove(temp_manifest_path);
        return false;
    }

    // Download and extract pack
    if (!downloadAndExtractPack(pack_url, gameDir, debug, log_file)) {
        fs::remove(temp_manifest_path);
        return false;
    }

    // Update version
    pack_version = remote_version;
    log("Pack updated to " + pack_version + ".", debug, log_file);

    fs::remove(temp_manifest_path);
    return true;
}

// Helper function to cleanup directories for update
bool cleanupDirectoriesForUpdate(const std::string& gameDir, bool debug, const std::string& log_file) {
    // Define folders to delete with better container
    const std::vector<std::string> foldersToDelete = {"config", "fancymenu_data", "mods", "shaderpacks"};

    // Remove servers.dat file
    const std::string serversFile = gameDir + "servers.dat";
    if (fs::exists(serversFile)) {
        try {
            fs::remove(serversFile);
            log("Deleted servers.dat for mandatory overwrite.", debug, log_file);
        } catch (const fs::filesystem_error& e) {
            log("Failed to delete servers.dat: " + std::string(e.what()), debug, log_file);
        }
    }

    // Remove directories
    for (const auto& folder : foldersToDelete) {
        const std::string dirPath = gameDir + folder + "/";
        if (fs::exists(dirPath)) {
            try {
                fs::remove_all(dirPath);
                log("Deleted " + folder + " folder for mandatory overwrite.", debug, log_file);
            } catch (const fs::filesystem_error& e) {
                log("Failed to delete " + folder + ": " + std::string(e.what()), debug, log_file);
                // Continue with other folders
            }
        }
    }

    return true;
}

// Helper function to download and extract pack
bool downloadAndExtractPack(const std::string& pack_url, const std::string& gameDir,
                           bool debug, const std::string& log_file) {
    const std::string pack_path = gameDir + "pack.zip";

    log("Downloading updated pack from " + pack_url + "...", debug, log_file);
    if (!downloadFile(pack_url, pack_path)) {
        log("Failed to download pack.", debug, log_file);
        return false;
    }

    log("Extracting pack...", debug, log_file);
    if (!extractArchive(pack_path, gameDir)) {
        log("Failed to extract pack.", debug, log_file);
        fs::remove(pack_path);
        return false;
    }

    fs::remove(pack_path);
    return true;
}