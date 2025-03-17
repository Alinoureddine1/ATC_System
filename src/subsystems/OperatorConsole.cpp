#include "OperatorConsole.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils.h"

OperatorConsole::OperatorConsole(std::vector<Plane>& planes, DataDisplay* dataDisplay, 
                                 CommunicationSystem* commSystem, const std::string& logPath)
    : planes(planes), dataDisplay(dataDisplay), commSystem(commSystem), commandLogPath(logPath) {}

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
    if (commSystem) {
        commSystem->send(command.planeId, command);
    } else {
        std::cout << printTimeStamp() << " No Communication System available\n";
        return;
    }
    // Log the command
    std::stringstream ss;
    ss << "Command for Plane " << command.planeId << ": code=" << command.code 
       << ", values=(" << command.value[0] << ", " << command.value[1] << ", " << command.value[2] << ")";
    logCommand(ss.str());
}

void OperatorConsole::requestPlaneInfo(int planeId) {
    for (const auto& plane : planes) {
        if (plane.getId() == planeId) {
            std::cout << printTimeStamp() << " Plane " << planeId << " Info:\n";
            std::cout << "  Position: (" << plane.getX() << ", " << plane.getY() << ", " << plane.getZ() << ")\n";
            std::cout << "  Velocity: (" << plane.getVx() << ", " << plane.getVy() << ", " << plane.getVz() << ")\n";
            std::cout << "  Flight Level: " << static_cast<int>(plane.getZ() / 100) << " (FL)\n";
            if (dataDisplay) {
                dataDisplay->requestAugmentedInfo(planeId);
            }
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
        return false; // Continue simulation
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