#include "ComputerSystem.h"
#include "utils.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h> 

static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

int main() {
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logComputerSystemMessage("Subsystem starting");
    
    ComputerSystem cs;
    
    std::thread csThread([&cs]() {
        try {
            cs.run();
        } catch (const std::exception& e) {
            logComputerSystemMessage("Exception in computer system thread: " + 
                                  std::string(e.what()), LOG_ERROR);
        }
    });
    
    while (running) {
        sleep(1);
    }
    
    logComputerSystemMessage("Shutdown signal received", LOG_WARNING);
    
    if (csThread.joinable()) {
        csThread.join();
    }
    
    logComputerSystemMessage("Shutdown complete");
    return 0;
}