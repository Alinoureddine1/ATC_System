#include "AirspaceLogger.h"
#include <iostream>
#include <stdio.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"
#include "shm_utils.h"

AirspaceLogger::AirspaceLogger(const std::string &lp)
    : logPath(lp), chid(-1)
{
    logAirspaceLoggerMessage("AirspaceLogger initialized with log path: " + lp);
}

void AirspaceLogger::registerChannelId() {
    pid = getpid(); 
    
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds* channels) {
            channels->loggerChid = chid;
            channels->loggerPid = pid;  
            logAirspaceLoggerMessage("Registered actual channel ID " + std::to_string(chid) + 
                                   " with PID " + std::to_string(pid) + " in shared memory");
        }
    );
    
    if (!success) {
        logAirspaceLoggerMessage("Failed to register channel ID in shared memory", LOG_ERROR);
    }
}

void AirspaceLogger::logAirspaceState(const std::vector<Position> &positions,
                                    const std::vector<Velocity> &velocities,
                                    double timestamp)
{
    ensureLogDirectories();

    FILE *fp = fopen(logPath.c_str(), "a");
    if (!fp) {
        logAirspaceLoggerMessage("Cannot open log file: " + logPath, LOG_ERROR);
        return;
    }
    
    fprintf(fp, "%s Logging at t=%.1f\n", printTimeStamp().c_str(), timestamp);
    
    for (size_t i = 0; i < positions.size(); i++) {
        fprintf(fp, " Plane %d => (%.1f,%.1f,%.1f) / vel(%.1f,%.1f,%.1f)\n",
                positions[i].planeId,
                positions[i].x, positions[i].y, positions[i].z,
                velocities[i].vx, velocities[i].vy, velocities[i].vz);
    }
    
    fprintf(fp, "------------------------\n");
    fclose(fp);
    
    logAirspaceLoggerMessage("Logged " + std::to_string(positions.size()) + 
                           " planes at timestamp " + std::to_string(timestamp));
}

void AirspaceLogger::run() {
    ensureLogDirectories();
    
    chid = ChannelCreate(0);
    if (chid == -1) {
        logAirspaceLoggerMessage("ChannelCreate failed: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        return;
    }
    
    logAirspaceLoggerMessage("Channel created with ID: " + std::to_string(chid));
    
    registerChannelId();

    AirspaceLogMessage msg;
    
    while (true) {
        memset(&msg, 0, sizeof(msg));
        
        int rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            logAirspaceLoggerMessage("MsgReceive failed: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
            sleep(1);  
            continue;
        }
        
        logAirspaceLoggerMessage("Received message with command type: " + 
                               std::to_string(msg.commandType), LOG_DEBUG);
        
        if (msg.commandType == COMMAND_LOG_AIRSPACE) {
            MsgReply(rcvid, EOK, nullptr, 0);
            
            logAirspaceLoggerMessage("Received airspace log request for timestamp " + 
                                   std::to_string(msg.timestamp));
            
            if (msg.numPlanes > 0 && msg.numPlanes <= MAX_PLANES) {
                std::vector<Position> positions;
                std::vector<Velocity> velocities;
                
                for (int i = 0; i < msg.numPlanes; i++) {
                    positions.push_back(msg.positions[i]);
                    velocities.push_back(msg.velocities[i]);
                }
                
                logAirspaceState(positions, velocities, msg.timestamp);
            } else {
                logAirspaceLoggerMessage("Received invalid plane count: " + 
                                      std::to_string(msg.numPlanes), LOG_WARNING);
            }
        } else if (msg.commandType == COMMAND_EXIT_THREAD) {
            logAirspaceLoggerMessage("Received exit command");
            MsgReply(rcvid, EOK, nullptr, 0);
            break;
        } else {
            logAirspaceLoggerMessage("Unknown command type: " + 
                                   std::to_string(msg.commandType), LOG_WARNING);
            MsgError(rcvid, ENOSYS);
        }
    }
    
    ChannelDestroy(chid);
    logAirspaceLoggerMessage("AirspaceLogger shutdown complete");
}

void* AirspaceLogger::start(void* context) {
    auto a = static_cast<AirspaceLogger*>(context);
    a->run();
    return nullptr;
}