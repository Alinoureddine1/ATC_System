#include "CommunicationSystem.h"
#include "utils.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h> // For sleep function

/**
 * CommunicationSystem reads /shm_commands in a loop, sending commands to planes.
 */

// Signal handler for clean shutdown
static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

int main() {
    // Set up signal handler for graceful termination
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logCommunicationSystemMessage("Subsystem starting");
    
    // Create the CommunicationSystem
    CommunicationSystem comm;
    
    // Start the communication system in a detached thread so we can handle signals
    std::thread commThread([&comm]() {
        try {
            comm.run(); // infinite loop reading commands
        } catch (const std::exception& e) {
            logCommunicationSystemMessage("Exception in communication thread: " + 
                                       std::string(e.what()), LOG_ERROR);
        }
    });
    
    // Wait for termination signal
    while (running) {
        sleep(1); // Use sleep(1) instead of pause() for better portability
    }
    
    logCommunicationSystemMessage("Shutdown signal received", LOG_WARNING);
    
    
    if (commThread.joinable()) {
        commThread.join();
    }
    
    logCommunicationSystemMessage("Shutdown complete");
    return 0;
}