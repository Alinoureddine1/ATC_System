#include "AirspaceLogger.h"
#include "utils.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h> // For sleep function

// Signal handler for clean shutdown
static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

int main() {
    // Set up signal handler for graceful termination
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logAirspaceLoggerMessage("Subsystem starting");
    
    // Create the AirspaceLogger
    AirspaceLogger al;
    
    // Start the logger in a detached thread so we can handle signals
    std::thread loggerThread(&AirspaceLogger::run, &al);
    
    // Wait for termination signal
    while (running) {
        sleep(1); // Use sleep(1) instead of pause() for better portability
    }
    
    logAirspaceLoggerMessage("Shutdown signal received", LOG_WARNING);
    

    if (loggerThread.joinable()) {
        loggerThread.join();
    }
    
    logAirspaceLoggerMessage("Shutdown complete");
    return 0;
}