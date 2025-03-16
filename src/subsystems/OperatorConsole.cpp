#include "OperatorConsole.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils.h"

OperatorConsole::OperatorConsole(std::vector<Plane>& planes, const std::string& logPath)
    : planes(planes), commandLogPath(logPath) {}

void OperatorConsole::logCommand(const std::string& command) {
    std::ofstream logFile(commandLogPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << printTimeStamp() << " " << command << "\n";
        logFile.close();
    } else {
        std::cerr << printTimeStamp() << " Error opening command log file: " << commandLogPath << "\n";
    }
}

void OperatorConsole::sendCommand(const Command& command) {
    // Find the plane by ID
    for (auto& plane : planes) {
        if (plane.getId() == command.planeId) {
            switch (command.code) {
                case CMD_VELOCITY:
                    // Update velocity
                    plane = Plane(plane.getId(), plane.getX(), plane.getY(), plane.getZ(),
                                  command.value[0], command.value[1], command.value[2]);
                    std::cout << printTimeStamp() << " Updated Plane " << command.planeId 
                              << " velocity to (" << command.value[0] << ", " 
                              << command.value[1] << ", " << command.value[2] << ")\n";
                    break;
                case CMD_POSITION:
                    // Update position
                    plane = Plane(plane.getId(), command.value[0], command.value[1], command.value[2],
                                  plane.getVx(), plane.getVy(), plane.getVz());
                    std::cout << printTimeStamp() << " Updated Plane " << command.planeId 
                              << " position to (" << command.value[0] << ", " 
                              << command.value[1] << ", " << command.value[2] << ")\n";
                    break;
                default:
                    std::cout << printTimeStamp() << " Unsupported command code: " << command.code << "\n";
                    return;
            }
            // Log the command
            std::stringstream ss;
            ss << "Command for Plane " << command.planeId << ": code=" << command.code 
               << ", values=(" << command.value[0] << ", " << command.value[1] << ", " << command.value[2] << ")";
            logCommand(ss.str());
            return;
        }
    }
    std::cout << printTimeStamp() << " Plane " << command.planeId << " not found\n";
}

void OperatorConsole::requestPlaneInfo(int planeId) {
    for (const auto& plane : planes) {
        if (plane.getId() == planeId) {
            std::cout << printTimeStamp() << " Plane " << planeId << " Info:\n";
            std::cout << "  Position: (" << plane.getX() << ", " << plane.getY() << ", " << plane.getZ() << ")\n";
            std::cout << "  Velocity: (" << plane.getVx() << ", " << plane.getVy() << ", " << plane.getVz() << ")\n";
            std::cout << "  Flight Level: " << static_cast<int>(plane.getZ() / 100) << " (FL)\n";
            // Log the request
            std::stringstream ss;
            ss << "Requested info for Plane " << planeId;
            logCommand(ss.str());
            return;
        }
    }
    std::cout << printTimeStamp() << " Plane " << planeId << " not found\n";
}

void OperatorConsole::displayMenu() {
    std::cout << "\nOperator Console Menu:\n";
    std::cout << "1. Send Velocity Command (e.g., change speed)\n";
    std::cout << "2. Send Position Command (e.g., change position)\n";
    std::cout << "3. Request Plane Info\n";
    std::cout << "4. Continue Simulation\n";
    std::cout << "Enter choice (1-4): ";
}

bool OperatorConsole::processInput() {
    int choice;
    std::cin >> choice;

    if (choice == 4) {
        return false; 
    }

    if (choice == 1 || choice == 2) {
        int planeId;
        double v1, v2, v3;
        std::cout << "Enter Plane ID: ";
        std::cin >> planeId;
        std::cout << "Enter values (e.g., vx vy vz for velocity, or x y z for position): ";
        std::cin >> v1 >> v2 >> v3;

        Command command;
        command.planeId = planeId;
        command.code = (choice == 1) ? CMD_VELOCITY : CMD_POSITION;
        command.value[0] = v1;
        command.value[1] = v2;
        command.value[2] = v3;
        command.timestamp = time(nullptr);

        sendCommand(command);
    } else if (choice == 3) {
        int planeId;
        std::cout << "Enter Plane ID: ";
        std::cin >> planeId;
        requestPlaneInfo(planeId);
    } else {
        std::cout << printTimeStamp() << " Invalid choice. Try again.\n";
    }
    return true; 
}