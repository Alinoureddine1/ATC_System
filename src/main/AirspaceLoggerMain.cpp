#include "AirspaceLogger.h"
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
    
    logAirspaceLoggerMessage("Subsystem starting");
    
    AirspaceLogger al;
    
    std::thread loggerThread(&AirspaceLogger::run, &al);
    
    while (running) {
        sleep(1); 
    }
    
    logAirspaceLoggerMessage("Shutdown signal received", LOG_WARNING);
    

    if (loggerThread.joinable()) {
        loggerThread.join();
    }
    
    logAirspaceLoggerMessage("Shutdown complete");
    return 0;
}