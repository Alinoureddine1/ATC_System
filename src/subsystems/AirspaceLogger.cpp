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

void AirspaceLogger::registerChannelId()
{
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds* channels) {

            // Register channel ID
            channels->loggerChid = AIRSPACE_LOGGER_CHANNEL_ID;
            logAirspaceLoggerMessage("Registered channel ID " + std::to_string(chid) + 
                                   " in shared memory");
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
    // Ensure directory exists
    ensureLogDirectories();

    // Open log file
    FILE *fp = fopen(logPath.c_str(), "a");
    if (!fp) {
        logAirspaceLoggerMessage("Cannot open log file: " + logPath, LOG_ERROR);
        return;
    }
    
    // Write timestamp
    fprintf(fp, "%s Logging at t=%.1f\n", printTimeStamp().c_str(), timestamp);
    
    // Log each plane's position and velocity
    for (size_t i = 0; i < positions.size(); i++) {
        fprintf(fp, " Plane %d => (%.1f,%.1f,%.1f) / vel(%.1f,%.1f,%.1f)\n",
                positions[i].planeId,
                positions[i].x, positions[i].y, positions[i].z,
                velocities[i].vx, velocities[i].vy, velocities[i].vz);
    }
    
    // Add a separator
    fprintf(fp, "------------------------\n");
    fclose(fp);
    
    logAirspaceLoggerMessage("Logged " + std::to_string(positions.size()) + 
                           " planes at timestamp " + std::to_string(timestamp));
}

void AirspaceLogger::run() {
    // Ensure log directory exists
    ensureLogDirectories();
    
    // Create a channel for receiving messages
    chid = ChannelCreate(3);
    if (chid == -1) {
        logAirspaceLoggerMessage("ChannelCreate failed: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        return;
    }
    
    logAirspaceLoggerMessage("Channel created with ID: " + std::to_string(chid));
    
    // Register our channel ID in shared memory
    registerChannelId();

    // Main message processing loop
    AirspaceLogMessage msg;
    while (true) {
        int rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            logAirspaceLoggerMessage("MsgReceive failed: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
            continue;
        }
        
        // Process the message
        if (msg.commandType == COMMAND_LOG_AIRSPACE) {
            logAirspaceLoggerMessage("Received airspace log request for timestamp " + 
                                   std::to_string(msg.timestamp));
            logAirspaceState(msg.positions, msg.velocities, msg.timestamp);
            MsgReply(rcvid, EOK, nullptr, 0);
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
    
    // Clean up
    ChannelDestroy(chid);
    logAirspaceLoggerMessage("AirspaceLogger shutdown complete");
}

void* AirspaceLogger::start(void* context) {
    auto a = static_cast<AirspaceLogger*>(context);
    a->run();
    return nullptr;
}