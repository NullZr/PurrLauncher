#include "include/logging.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <memory>

namespace fs = std::filesystem;


// Thread-safe singleton logger class with RAII
class Logger {
private:
    std::unique_ptr<std::ofstream> logFile;
    std::mutex logMutex;
    std::string currentLogPath;
    bool debugMode;

    // Private constructor for singleton
    Logger() : debugMode(false) {}

public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void initialize(const std::string& log_file_path, bool debug) {
        std::lock_guard<std::mutex> lock(logMutex);

        debugMode = debug;

        if (debugMode && log_file_path != currentLogPath) {
            // Close existing file if open
            if (logFile && logFile->is_open()) {
                logFile->close();
            }

            // Create parent directories if they don't exist
            if (const auto parent = fs::path(log_file_path).parent_path(); !parent.empty()) {
                try {
                    fs::create_directories(parent);
                } catch (const fs::filesystem_error& e) {
                    std::cerr << "Failed to create log directory: " << e.what() << std::endl;
                    return;
                }
            }

            // Open new log file
            logFile = std::make_unique<std::ofstream>(log_file_path, std::ios::app);
            if (!logFile->is_open()) {
                std::cerr << "Failed to open log file: " << log_file_path << std::endl;
                logFile.reset();
                return;
            }

            currentLogPath = log_file_path;

            // Write session start marker
            writeLogEntry("=== New session started ===", LogLevel::INFO, false);
        }
    }

    void writeLog(const std::string& message, LogLevel level = LogLevel::INFO) {
        std::lock_guard<std::mutex> lock(logMutex);

        // Always output to console
        std::cout << formatMessage(message, level) << std::endl;

        // Write to file if debug mode is enabled
        if (debugMode && logFile && logFile->is_open()) {
            writeLogEntry(message, level, true);
            logFile->flush(); // Ensure immediate write for debugging
        }
    }

    void close() {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile && logFile->is_open()) {
            writeLogEntry("=== Session ended ===", LogLevel::INFO, false);
            logFile->close();
        }
        logFile.reset();
        currentLogPath.clear();
    }

    ~Logger() {
        close();
    }

private:

    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string getLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERR: return "ERROR";
            case LogLevel::DEBUG: return "DEBUG";
            default: return "UNKNOWN";
        }
    }

    std::string formatMessage(const std::string& message, LogLevel level) const {
        return "[" + getCurrentTimestamp() + "] [" + getLevelString(level) + "] " + message;
    }

    void writeLogEntry(const std::string& message, LogLevel level, bool includeTimestamp) {
        if (logFile && logFile->is_open()) {
            if (includeTimestamp) {
                *logFile << formatMessage(message, level) << std::endl;
            } else {
                *logFile << message << std::endl;
            }
        }
    }
};

// Global log file handle for backward compatibility
std::ofstream logFile;

void log(const std::string& msg, bool debug, const std::string& log_file_path) {
    static std::once_flag initialized;
    std::call_once(initialized, [&]() {
        Logger::getInstance().initialize(log_file_path, debug);
    });

    Logger::getInstance().writeLog(msg);
}

// Enhanced logging functions
void logInfo(const std::string& message, bool debug, const std::string& log_file_path) {
    log("[INFO] " + message, debug, log_file_path);
}

void logWarning(const std::string& message, bool debug, const std::string& log_file_path) {
    log("[WARNING] " + message, debug, log_file_path);
}

void logError(const std::string& message, bool debug, const std::string& log_file_path) {
    log("[ERROR] " + message, debug, log_file_path);
}

void logDebug(const std::string& message, bool debug, const std::string& log_file_path) {
    if (debug) {
        log("[DEBUG] " + message, debug, log_file_path);
    }
}

// Performance logging
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::string operationName;
    bool debugMode;
    std::string logPath;

public:
    PerformanceTimer(const std::string& operation, bool debug, const std::string& log_file_path)
        : startTime(std::chrono::high_resolution_clock::now())
        , operationName(operation)
        , debugMode(debug)
        , logPath(log_file_path) {

        if (debugMode) {
            logDebug("Starting: " + operationName, debugMode, logPath);
        }
    }

    ~PerformanceTimer() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        std::stringstream ss;
        ss << "Completed: " << operationName << " (took " << duration.count() << "ms)";

        if (debugMode) {
            logDebug(ss.str(), debugMode, logPath);
        }
    }
};

// Macro for easy performance timing
#define LOG_PERFORMANCE(operation, debug, log_path) \
    PerformanceTimer timer(operation, debug, log_path)

// Initialize logging system
void initializeLogging(const std::string& log_file_path, bool debug) {
    Logger::getInstance().initialize(log_file_path, debug);
}

// Cleanup logging system
void cleanupLogging() {
    Logger::getInstance().close();
}

// Log system information
void logSystemInfo(bool debug, const std::string& log_file_path) {
    if (!debug) return;

    logInfo("=== System Information ===", debug, log_file_path);

    // Log current working directory
    try {
        std::string cwd = fs::current_path().string();
        logInfo("Working Directory: " + cwd, debug, log_file_path);
    } catch (const fs::filesystem_error& e) {
        logWarning("Failed to get working directory: " + std::string(e.what()), debug, log_file_path);
    }

    // Log available disk space (approximate)
    try {
        auto space = fs::space(fs::current_path());
        double availableGB = static_cast<double>(space.available) / (1024 * 1024 * 1024);
        std::stringstream ss;
        ss << "Available disk space: " << std::fixed << std::setprecision(2) << availableGB << " GB";
        logInfo(ss.str(), debug, log_file_path);
    } catch (const fs::filesystem_error& e) {
        logWarning("Failed to get disk space: " + std::string(e.what()), debug, log_file_path);
    }

    logInfo("=== End System Information ===", debug, log_file_path);
}