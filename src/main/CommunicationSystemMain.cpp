#include "CommunicationSystem.h"
#include "utils.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h>

/**
 * CommunicationSystem reads /shm_commands in a loop, sending commands to planes.
 */

static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

int main() {
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logCommunicationSystemMessage("Subsystem starting");
    
    CommunicationSystem comm;
    
    std::thread commThread([&comm]() {
        try {
            comm.run(); // infinite loop reading commands
        } catch (const std::exception& e) {
            logCommunicationSystemMessage("Exception in communication thread: " + 
                                       std::string(e.what()), LOG_ERROR);
        }
    });
    
    while (running) {
        sleep(1); 
    }
    
    logCommunicationSystemMessage("Shutdown signal received", LOG_WARNING);
    
    
    if (commThread.joinable()) {
        commThread.join();
    }
    
    logCommunicationSystemMessage("Shutdown complete");
    return 0;
}