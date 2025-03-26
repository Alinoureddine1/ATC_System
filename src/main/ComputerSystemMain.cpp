#include "ComputerSystem.h"
#include "utils.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h> 

// Signal handler for clean shutdown
static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

int main() {
    // Set up signal handler for graceful termination
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logComputerSystemMessage("Subsystem starting");
    
    // Create the ComputerSystem with default prediction time
    ComputerSystem cs;
    
    // Start the computer system in a detached thread so we can handle signals
    std::thread csThread([&cs]() {
        try {
            cs.run();
        } catch (const std::exception& e) {
            logComputerSystemMessage("Exception in computer system thread: " + 
                                  std::string(e.what()), LOG_ERROR);
        }
    });
    
    // Wait for termination signal
    while (running) {
        sleep(1);
    }
    
    logComputerSystemMessage("Shutdown signal received", LOG_WARNING);
    
    // Clean up and exit
    // Note: In a more complete implementation, we'd need a way to signal
    // the ComputerSystem's run() method to terminate
    
    if (csThread.joinable()) {
        csThread.join();
    }
    
    logComputerSystemMessage("Shutdown complete");
    return 0;
}