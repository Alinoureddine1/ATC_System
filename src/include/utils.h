#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <time.h>
#include <iostream>
#include <fstream>
#include <mutex>
#include <cstdio>
#include <sys/stat.h>

/** Airspace boundaries **/
const double AIRSPACE_X_MIN = 0.0;
const double AIRSPACE_X_MAX = 100000.0;
const double AIRSPACE_Y_MIN = 0.0;
const double AIRSPACE_Y_MAX = 100000.0;
const double AIRSPACE_Z_MIN = 0.0;
const double AIRSPACE_Z_MAX = 25000.0;

/** Log levels for finer-grained control **/
enum LogLevel {
    LOG_DEBUG,    // Detailed debugging information
    LOG_INFO,     // General informational messages
    LOG_WARNING,  // Warning conditions
    LOG_ERROR     // Error conditions
};

// Global log level setting
static LogLevel currentLogLevel = LOG_INFO;

/** Set global log level **/
static inline void setLogLevel(LogLevel level) {
    currentLogLevel = level;
}

/** Convert bool->string **/
static inline std::string boolToString(bool value) {
    return value ? "true" : "false";
}

/** Check if within airspace bounds **/
static inline bool isPositionWithinBounds(double x, double y, double z) {
    return (x >= AIRSPACE_X_MIN && x <= AIRSPACE_X_MAX &&
            y >= AIRSPACE_Y_MIN && y <= AIRSPACE_Y_MAX &&
            z >= AIRSPACE_Z_MIN && z <= AIRSPACE_Z_MAX);
}

/** Print a timestamp for logging **/
static inline std::string printTimeStamp() {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", localtime(&now));
    return std::string(buf);
}

/** Ensure log directories exist **/
static inline void ensureLogDirectories() {
    static std::once_flag dirFlag;
    std::call_once(dirFlag, []() {
        mkdir("/tmp/atc", 0777);
        mkdir("/tmp/atc/logs", 0777);
    });
}

/** Enhanced logging function with log levels **/
static inline void logMessage(const std::string& subsystem, 
                            const std::string& message,
                            const std::string& logPath, 
                            LogLevel level = LOG_INFO) {
    // Skip if message level is below current log level
    if (level < currentLogLevel) return;
    
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string levelStr;
    switch (level) {
        case LOG_DEBUG:   levelStr = "DEBUG"; break;
        case LOG_INFO:    levelStr = "INFO"; break;
        case LOG_WARNING: levelStr = "WARNING"; break;
        case LOG_ERROR:   levelStr = "ERROR"; break;
    }
    
    std::string timestampedMsg = printTimeStamp() + " [" + subsystem + "] [" + levelStr + "] " + message;
    
    // Console output based on log level
    if (level >= LOG_WARNING) {
        std::cerr << timestampedMsg << std::endl;
    } else {
        std::cout << timestampedMsg << std::endl;
    }
    
    // Ensure log directory exists
    ensureLogDirectories();
    
    // Log to file
    FILE* fp = fopen(logPath.c_str(), "a");
    if (fp) {
        fprintf(fp, "%s\n", timestampedMsg.c_str());
        fclose(fp);
    } else {
        // Only print this error to console to avoid recursive logging issues
        std::cerr << "Failed to open log file: " << logPath << std::endl;
    }
}

/** Subsystem-specific logging functions **/
static inline void logRadarMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("Radar", message, "/tmp/atc/logs/radar.log", level);
}

static inline void logComputerSystemMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("ComputerSystem", message, "/tmp/atc/logs/computer_system.log", level);
}

static inline void logOperatorConsoleMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("OperatorConsole", message, "/tmp/atc/logs/operator_console.log", level);
}

static inline void logDataDisplayMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("DataDisplay", message, "/tmp/atc/logs/data_display.log", level);
}

static inline void logAirspaceLoggerMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("AirspaceLogger", message, "/tmp/atc/logs/airspace_logger.log", level);
}

static inline void logCommunicationSystemMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("CommunicationSystem", message, "/tmp/atc/logs/communication_system.log", level);
}

static inline void logPlaneMessage(int planeId, const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("Plane-" + std::to_string(planeId), message, "/tmp/atc/logs/plane.log", level);
}

static inline void logSystemMessage(const std::string& message, LogLevel level = LOG_INFO) {
    logMessage("ATCController", message, "/tmp/atc/logs/system.log", level);
}

#endif // UTILS_H