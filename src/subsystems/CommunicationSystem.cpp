#include "CommunicationSystem.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cmath>
#include <sys/stat.h>
#include "utils.h"
#include "shm_utils.h"

CommunicationSystem::CommunicationSystem(const std::string& logPath)
    : transmissionLogPath(logPath)
{
    logCommunicationSystemMessage("CommunicationSystem initialized");
}


void CommunicationSystem::send(int planeId, const Command& command) {
    std::string msg = "Direct send to plane=" + std::to_string(planeId)
                    + " code=" + std::to_string(command.code)
                    + " val(" + std::to_string(command.value[0]) + ","
                              + std::to_string(command.value[1]) + ","
                              + std::to_string(command.value[2]) + ")";
    logTransmission(msg);
    logCommunicationSystemMessage(msg);
}

void CommunicationSystem::logTransmission(const std::string& message) {
    ensureLogDirectories();
    
    FILE* fp = fopen(transmissionLogPath.c_str(), "a");
    if (fp) {
        fprintf(fp, "%s %s\n", printTimeStamp().c_str(), message.c_str());
        fclose(fp);
    } else {
        logCommunicationSystemMessage("Failed to open transmission log: " + transmissionLogPath, LOG_ERROR);
    }
}

void CommunicationSystem::run() {
    logCommunicationSystemMessage("Communication system starting");
    
    while (true) {
        bool success = accessSharedMemory<CommandQueue>(
            SHM_COMMANDS, 
            sizeof(CommandQueue), 
            O_RDWR, 
            false,
            [this](CommandQueue* cq) {
                if (cq->head != cq->tail) {
                    Command cmd = cq->commands[cq->head];
                    cq->head = (cq->head + 1) % MAX_COMMANDS;
                    send(cmd.planeId, cmd);
                } else {
                    usleep(100000); 
                }
            }
        );
        
        if (!success) {
            logCommunicationSystemMessage("Failed to access command queue, retrying...", LOG_WARNING);
            sleep(1);
        }
    }
}