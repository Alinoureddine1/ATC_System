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
    pid = getpid();  
    
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds* channels) {
            channels->displayChid = chid;
            channels->displayPid = pid;  
            logDataDisplayMessage("Registered actual channel ID " + std::to_string(chid) + 
                               " with PID " + std::to_string(pid) + " in shared memory");
        }
    );
    
    if (!success) {
        logDataDisplayMessage("Failed to register channel ID in shared memory", LOG_ERROR);
    }
}

void DataDisplay::run() {
    ensureLogDirectories();
    
    chid = ChannelCreate(0);
    
    if (chid == -1) {
        logDataDisplayMessage("ChannelCreate failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }
    
    logDataDisplayMessage("Channel created with ID: " + std::to_string(chid));
    
    registerChannelId();
    
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
    struct {
        dataDisplayCommandMessage msg;
    } buffer;
    
    int rcvid;
    
    logDataDisplayMessage("Starting message processing loop");
    
    while (true) {
        memset(&buffer, 0, sizeof(buffer));
        
        rcvid = MsgReceive(chid, &buffer, sizeof(buffer), nullptr);
        if (rcvid == -1) {
            logDataDisplayMessage("MsgReceive failed: " + std::string(strerror(errno)), LOG_ERROR);
            sleep(1); 
            continue;
        }
        
        dataDisplayCommandMessage& msg = buffer.msg;
        
        logDataDisplayMessage("Received message with command type: " + std::to_string(msg.commandType), LOG_DEBUG);
        
        switch (msg.commandType) {
            case COMMAND_ONE_PLANE: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                logDataDisplayMessage("Processing COMMAND_ONE_PLANE for aircraft: " + 
                                   std::to_string(msg.commandBody.one.aircraftID), LOG_DEBUG);
                
                if (isfinite(msg.commandBody.one.position.x) && 
                    isfinite(msg.commandBody.one.position.y) && 
                    isfinite(msg.commandBody.one.position.z) &&
                    isfinite(msg.commandBody.one.velocity.x) && 
                    isfinite(msg.commandBody.one.velocity.y) && 
                    isfinite(msg.commandBody.one.velocity.z)) {
                    
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
            
            case COMMAND_MULTIPLE_PLANE:
            case COMMAND_GRID:
            case COMMAND_LOG: {
                MsgReply(rcvid, EOK, nullptr, 0);
                
                if (msg.commandBody.multiple.numberOfAircrafts > MAX_PLANES) {
                    logDataDisplayMessage("Invalid number of aircraft: " + 
                                       std::to_string(msg.commandBody.multiple.numberOfAircrafts), LOG_ERROR);
                    break;
                }
                
                std::string cmdTypeStr;
                if (msg.commandType == COMMAND_GRID) {
                    cmdTypeStr = "GRID";
                } else if (msg.commandType == COMMAND_LOG) {
                    cmdTypeStr = "LOG";
                } else {
                    cmdTypeStr = "MULTIPLE_PLANE";
                }
                
                logDataDisplayMessage("Processing " + cmdTypeStr + " for " + 
                                   std::to_string(msg.commandBody.multiple.numberOfAircrafts) + " aircraft", LOG_DEBUG);
                
                // Handle each command type
                if (msg.commandType == COMMAND_MULTIPLE_PLANE) {
                    // Display info for each plane
                    for (size_t i = 0; i < msg.commandBody.multiple.numberOfAircrafts; i++) {
                        // Validate data before using
                        Vec3& pos = msg.commandBody.multiple.positionArray[i];
                        Vec3& vel = msg.commandBody.multiple.velocityArray[i];
                        int planeId = msg.commandBody.multiple.planeIDArray[i];
                        
                        if (!isfinite(pos.x) || !isfinite(pos.y) || !isfinite(pos.z) ||
                            !isfinite(vel.x) || !isfinite(vel.y) || !isfinite(vel.z)) {
                            logDataDisplayMessage("Invalid data for plane " + std::to_string(planeId), LOG_WARNING);
                            continue;
                        }
                        
                        std::string planeInfo = "Plane " + std::to_string(planeId) +
                            " pos(" + std::to_string(pos.x) + "," +
                                      std::to_string(pos.y) + "," +
                                      std::to_string(pos.z) + ")" +
                            " vel(" + std::to_string(vel.x) + "," +
                                      std::to_string(vel.y) + "," +
                                      std::to_string(vel.z) + ")";
                        
                        logDataDisplayMessage(planeInfo);
                    }
                } else if (msg.commandType == COMMAND_GRID) {
                    // Generate and display grid
                    std::string grid = generateGrid(msg.commandBody.multiple);
                    logDataDisplayMessage("GRID:\n" + grid);
                } else if (msg.commandType == COMMAND_LOG) {
                    // Generate grid and log it
                    std::string grid = generateGrid(msg.commandBody.multiple);
                    logDataDisplayMessage("LOG:\n" + grid);
                    
                    if (fd != -1) {
                        std::string s = printTimeStamp() + " LOG:\n" + grid + "\n";
                        if (write(fd, s.c_str(), s.size()) == -1) {
                            logDataDisplayMessage("Failed to write to log file: " + 
                                                std::string(strerror(errno)), LOG_ERROR);
                        }
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
    
    if (airspaceInfo.numberOfAircrafts > MAX_PLANES) {
        logDataDisplayMessage("Invalid airspace data: too many aircraft", LOG_ERROR);
        return "Error: Invalid data received";
    }
    
    // Place planes in grid
    for (size_t i = 0; i < airspaceInfo.numberOfAircrafts; i++) {
        // Validate data for this aircraft
        if (!isfinite(airspaceInfo.positionArray[i].x) || 
            !isfinite(airspaceInfo.positionArray[i].y)) {
            continue; 
        }
        
        double px = airspaceInfo.positionArray[i].x;
        double py = airspaceInfo.positionArray[i].y;
        int planeId = airspaceInfo.planeIDArray[i];

        // Find cell for this plane
        for (int r = 0; r < rowSize; r++) {
            double yMin = cell*r, yMax = cell*(r+1);
            if (py >= yMin && py < yMax) {
                for (int c = 0; c < colSize; c++) {
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
    for (int r = 0; r < rowSize; r++) {
        out << "\n";
        for (int c = 0; c < colSize; c++) {
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