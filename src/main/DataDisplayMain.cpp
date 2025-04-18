#include "DataDisplay.h"
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
    
    logDataDisplayMessage("Subsystem starting");
    
    DataDisplay dd;
    
    std::thread ddThread([&dd]() {
        try {
            dd.run();
        } catch (const std::exception& e) {
            logDataDisplayMessage("Exception in data display thread: " + 
                               std::string(e.what()), LOG_ERROR);
        }
    });
    
    while (running) {
        sleep(1);
    }
    
    logDataDisplayMessage("Shutdown signal received", LOG_WARNING);
    
    
    if (ddThread.joinable()) {
        ddThread.join();
    }
    
    logDataDisplayMessage("Shutdown complete");
    return 0;
}