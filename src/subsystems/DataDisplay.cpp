#include "DataDisplay.h"
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include "utils.h"
#include "shm_utils.h"
#include <math.h>

DataDisplay::DataDisplay(const std::string& logPath)
    : chid(-1), fd(-1), logPath(logPath)
{
    logDataDisplayMessage("DataDisplay initialized with log path: " + logPath);
}

int DataDisplay::getChid() const {
    return chid;
}

void DataDisplay::registerChannelId() {
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds* channels) {
            // Register channel ID
            channels->displayChid = DATA_DISPLAY_CHANNEL_ID;
            logDataDisplayMessage("Registered channel ID " + std::to_string(chid) + " in shared memory");
        }
    );
    
    if (!success) {
        logDataDisplayMessage("Failed to register channel ID in shared memory", LOG_ERROR);
    }
}

void DataDisplay::run() {
    // Ensure log directory exists
    ensureLogDirectories();
    
    chid = ChannelCreate(DATA_DISPLAY_CHANNEL_ID);
    
    if (chid == -1) {
        logDataDisplayMessage("ChannelCreate failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }
    
    logDataDisplayMessage("Channel created with ID: " + std::to_string(chid));
    
    // Register our channel ID in shared memory
    registerChannelId();
    
    // Open log file
    fd = open(logPath.c_str(), O_CREAT|O_WRONLY|O_APPEND, 0666);
    if (fd == -1) {
        logDataDisplayMessage("Failed to open log file: " + std::string(strerror(errno)), LOG_ERROR);
    } else {
        logDataDisplayMessage("Log file opened successfully: " + logPath);
    }

    receiveMessage();

    if (fd != -1) {
        close(fd);
    }
    ChannelDestroy(chid);
    
    logDataDisplayMessage("DataDisplay shutdown complete");
}

void DataDisplay::receiveMessage() {
    dataDisplayCommandMessage msg;
    int rcvid;
    
    logDataDisplayMessage("Starting message processing loop");
    
    while (true) {
        // Zero out the message buffer before receiving
        memset(&msg, 0, sizeof(msg));
        
        rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            logDataDisplayMessage("MsgReceive failed: " + std::string(strerror(errno)), LOG_ERROR);
            sleep(1);  // Add delay before retry
            continue;
        }
        
        switch (msg.commandType) {
            case COMMAND_ONE_PLANE: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                // Validate data before using
                if (finite(msg.commandBody.one.position.x) && 
                    finite(msg.commandBody.one.position.y) && 
                    finite(msg.commandBody.one.position.z) &&
                    finite(msg.commandBody.one.velocity.x) && 
                    finite(msg.commandBody.one.velocity.y) && 
                    finite(msg.commandBody.one.velocity.z)) {
                    
                    std::string planeInfo = "Single plane " + 
                        std::to_string(msg.commandBody.one.aircraftID) +
                        " pos(" + std::to_string(msg.commandBody.one.position.x) + "," +
                                  std::to_string(msg.commandBody.one.position.y) + "," +
                                  std::to_string(msg.commandBody.one.position.z) + ")" +
                        " vel(" + std::to_string(msg.commandBody.one.velocity.x) + "," +
                                  std::to_string(msg.commandBody.one.velocity.y) + "," +
                                  std::to_string(msg.commandBody.one.velocity.z) + ")";
                    
                    logDataDisplayMessage(planeInfo);
                } else {
                    logDataDisplayMessage("Received invalid data for plane", LOG_WARNING);
                }
                break;
            }
            
            case COMMAND_MULTIPLE_PLANE: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                // Safety check for valid pointers and data
                if (msg.commandBody.multiple.planeIDArray == nullptr ||
                    msg.commandBody.multiple.positionArray == nullptr || 
                    msg.commandBody.multiple.velocityArray == nullptr ||
                    msg.commandBody.multiple.numberOfAircrafts > MAX_PLANES) {
                    
                    logDataDisplayMessage("Invalid multiple aircraft data received", LOG_ERROR);
                    break;
                }
                
                for (size_t i=0; i < msg.commandBody.multiple.numberOfAircrafts; i++) {
                    // Get refs to make code more readable
                    int pid = msg.commandBody.multiple.planeIDArray[i];
                    Vec3& pp = msg.commandBody.multiple.positionArray[i];
                    Vec3& vv = msg.commandBody.multiple.velocityArray[i];
                    
                    // Validate data before using
                    if (!finite(pp.x) || !finite(pp.y) || !finite(pp.z) ||
                        !finite(vv.x) || !finite(vv.y) || !finite(vv.z)) {
                        logDataDisplayMessage("Invalid data for plane " + std::to_string(pid), LOG_WARNING);
                        continue;
                    }
                    
                    std::string planeInfo = "Plane " + std::to_string(pid) +
                        " pos(" + std::to_string(pp.x) + "," +
                                  std::to_string(pp.y) + "," +
                                  std::to_string(pp.z) + ")" +
                        " vel(" + std::to_string(vv.x) + "," +
                                  std::to_string(vv.y) + "," +
                                  std::to_string(vv.z) + ")";
                    
                    logDataDisplayMessage(planeInfo);
                }
                break;
            }
            
            case COMMAND_GRID: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                // Safety check for valid data
                if (msg.commandBody.multiple.planeIDArray == nullptr ||
                    msg.commandBody.multiple.positionArray == nullptr || 
                    msg.commandBody.multiple.velocityArray == nullptr ||
                    msg.commandBody.multiple.numberOfAircrafts > MAX_PLANES) {
                    
                    logDataDisplayMessage("Invalid data for grid display", LOG_ERROR);
                    break;
                }
                
                std::string g = generateGrid(msg.commandBody.multiple);
                logDataDisplayMessage("GRID:\n" + g);
                break;
            }
            
            case COMMAND_LOG: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                // Safety check for valid data
                if (msg.commandBody.multiple.planeIDArray == nullptr ||
                    msg.commandBody.multiple.positionArray == nullptr || 
                    msg.commandBody.multiple.velocityArray == nullptr ||
                    msg.commandBody.multiple.numberOfAircrafts > MAX_PLANES) {
                    
                    logDataDisplayMessage("Invalid data for logging", LOG_ERROR);
                    break;
                }
                
                std::string g = generateGrid(msg.commandBody.multiple);
                logDataDisplayMessage("LOG:\n" + g);
                
                if (fd != -1) {
                    std::string s = printTimeStamp() + " LOG:\n" + g + "\n";
                    if (write(fd, s.c_str(), s.size()) == -1) {
                        logDataDisplayMessage("Failed to write to log file: " + 
                                            std::string(strerror(errno)), LOG_ERROR);
                    }
                }
                break;
            }
            
            case COMMAND_EXIT_THREAD:
                logDataDisplayMessage("Received exit command");
                MsgReply(rcvid, EOK, nullptr, 0);
                return;
                
            default:
                logDataDisplayMessage("Unknown command type: " + 
                                    std::to_string(msg.commandType), LOG_WARNING);
                MsgError(rcvid, ENOSYS);
                break;
        }
    }
}

std::string DataDisplay::generateGrid(const multipleAircraftDisplay& airspaceInfo) {
    constexpr int rowSize = 25;
    constexpr int colSize = 25;
    constexpr int cell = 4000;

    std::string grid[rowSize][colSize];
    
    // Validate input data before processing
    if (!airspaceInfo.planeIDArray || !airspaceInfo.positionArray || 
        !airspaceInfo.velocityArray || airspaceInfo.numberOfAircrafts > MAX_PLANES) {
        logDataDisplayMessage("Invalid airspace data received", LOG_ERROR);
        return "Error: Invalid data received";
    }
    
    // Place planes in grid
    for (size_t i=0; i < airspaceInfo.numberOfAircrafts; i++) {
        // Validate data for this aircraft
        if (!finite(airspaceInfo.positionArray[i].x) || 
            !finite(airspaceInfo.positionArray[i].y)) {
            continue;  // Skip invalid position data
        }
        
        double px = airspaceInfo.positionArray[i].x;
        double py = airspaceInfo.positionArray[i].y;
        int planeId = airspaceInfo.planeIDArray[i];

        // Find cell for this plane
        for (int r=0; r < rowSize; r++) {
            double yMin = cell*r, yMax = cell*(r+1);
            if (py >= yMin && py < yMax) {
                for (int c=0; c < colSize; c++) {
                    double xMin = cell*c, xMax = cell*(c+1);
                    if (px >= xMin && px < xMax) {
                        if (!grid[r][c].empty()) grid[r][c] += ",";
                        grid[r][c] += std::to_string(planeId);
                    }
                }
            }
        }
    }

    // Generate the grid as a string
    std::stringstream out;
    for (int r=0; r < rowSize; r++) {
        out << "\n";
        for (int c=0; c < colSize; c++) {
            if (grid[r][c].empty()) out << "| ";
            else out << "|" << grid[r][c];
        }
    }
    out << "\n";
    return out.str();
}

void DataDisplay::displayAirspace(double currentTime, const std::vector<Position>& positions) {
    logDataDisplayMessage("Airspace at t=" + std::to_string(currentTime) + "s");
    
    for (auto &p : positions) {
        logDataDisplayMessage("Plane " + std::to_string(p.planeId) + 
                            " => (" + std::to_string(p.x) + "," + 
                                      std::to_string(p.y) + "," + 
                                      std::to_string(p.z) + ")");
    }
}

void DataDisplay::requestAugmentedInfo(int planeId) {
    logDataDisplayMessage("Requesting augmented info for plane " + std::to_string(planeId));
}

void* DataDisplay::start(void* context) {
    auto dd = static_cast<DataDisplay*>(context);
    dd->run();
    return nullptr;
}