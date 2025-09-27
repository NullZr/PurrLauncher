#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include <fstream>

// Define LogLevel enum for enhanced logging
enum class LogLevel {
    INFO,
    WARNING,
    ERR,    // Changed from ERROR to ERR to avoid Windows macro conflict
    DEBUG
};

// Backward compatibility
extern std::ofstream logFile;

// Basic logging function (backward compatible)
void log(const std::string& msg, bool debug, const std::string& log_file_path);

// Enhanced logging functions
void logInfo(const std::string& message, bool debug, const std::string& log_file_path);
void logWarning(const std::string& message, bool debug, const std::string& log_file_path);
void logError(const std::string& message, bool debug, const std::string& log_file_path);
void logDebug(const std::string& message, bool debug, const std::string& log_file_path);

// System management functions
void initializeLogging(const std::string& log_file_path, bool debug);
void cleanupLogging();
void logSystemInfo(bool debug, const std::string& log_file_path);

// Performance timing class
class PerformanceTimer;

// Macro for performance timing
#define LOG_PERFORMANCE(operation, debug, log_path) \
PerformanceTimer timer(operation, debug, log_path)

#endif // LOGGING_H
