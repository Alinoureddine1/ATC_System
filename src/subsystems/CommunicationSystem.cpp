#include "CommunicationSystem.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils.h"

CommunicationSystem::CommunicationSystem(std::vector<Plane>& planes, const std::string& logPath)
    : planes(planes), transmissionLogPath(logPath) {}

void CommunicationSystem::logTransmission(const std::string& message) {
    std::ofstream logFile(transmissionLogPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << printTimeStamp() << " " << message << "\n";
        logFile.close();
    } else {
        std::cerr << printTimeStamp() << " Error opening transmission log file: " << transmissionLogPath << "\n";
    }
}

void CommunicationSystem::send(int planeId, const Command& command) {
    for (auto& plane : planes) {
        if (plane.getId() == planeId) {
            switch (command.code) {
                case CMD_VELOCITY:
                    plane = Plane(plane.getId(), plane.getX(), plane.getY(), plane.getZ(),
                                  command.value[0], command.value[1], command.value[2]);
                    std::cout << printTimeStamp() << " Transmitted to Plane " << planeId 
                              << ": Set velocity to (" << command.value[0] << ", " 
                              << command.value[1] << ", " << command.value[2] << ")\n";
                    break;
                case CMD_POSITION:
                    plane = Plane(plane.getId(), command.value[0], command.value[1], command.value[2],
                                  plane.getVx(), plane.getVy(), plane.getVz());
                    std::cout << printTimeStamp() << " Transmitted to Plane " << planeId 
                              << ": Set position to (" << command.value[0] << ", " 
                              << command.value[1] << ", " << command.value[2] << ")\n";
                    break;
                default:
                    std::cout << printTimeStamp() << " Unsupported command code: " << command.code << "\n";
                    return;
            }
            // Log the transmission
            std::stringstream ss;
            ss << "Transmitted to Plane " << planeId << ": code=" << command.code 
               << ", values=(" << command.value[0] << ", " << command.value[1] << ", " << command.value[2] << ")";
            logTransmission(ss.str());
            return;
        }
    }
    std::cout << printTimeStamp() << " Plane " << planeId << " not found for transmission\n";
}