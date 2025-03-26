#include "OperatorConsole.h"
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
    
    logOperatorConsoleMessage("Subsystem starting");
    
    // Create the OperatorConsole
    OperatorConsole opCon;
    
    // Start the operator console in a detached thread so we can handle signals
    std::thread opConThread([&opCon]() {
        try {
            opCon.run();
        } catch (const std::exception& e) {
            logOperatorConsoleMessage("Exception in operator console thread: " + 
                                   std::string(e.what()), LOG_ERROR);
        }
    });
    
    // Wait for termination signal
    while (running) {
        sleep(1); 
    }
    
    logOperatorConsoleMessage("Shutdown signal received", LOG_WARNING);
    
    
    if (opConThread.joinable()) {
        opConThread.join();
    }
    
    logOperatorConsoleMessage("Shutdown complete");
    return 0;
}