#include "DataDisplay.h"
#include <iostream>
#include "utils.h"

DataDisplay::DataDisplay(ComputerSystem& computerSystem, const std::vector<Plane>& planes)
    : computerSystem(computerSystem), planes(planes), requestedPlaneId(-1) {}

void DataDisplay::displayAirspace(double currentTime) {
    std::cout << printTimeStamp() << " Airspace Plan View (X-Y) at Time: " << currentTime << "s\n";
    std::cout << "--------------------------------------------------\n";

    auto positions = computerSystem.getDisplayData();
    for (const auto& pos : positions) {
        std::cout << "Plane " << pos.planeId << ": (X: " << pos.x << ", Y: " << pos.y << ", Z: " << pos.z << ")\n";
    }

    
    if (requestedPlaneId != -1) {
        for (const auto& plane : planes) {
            if (plane.getId() == requestedPlaneId) {
                std::cout << "Augmented Info for Plane " << requestedPlaneId << ":\n";
                std::cout << "  Position: (" << plane.getX() << ", " << plane.getY() << ", " << plane.getZ() << ")\n";
                std::cout << "  Velocity: (" << plane.getVx() << ", " << plane.getVy() << ", " << plane.getVz() << ")\n";
                std::cout << "  Flight Level: " << static_cast<int>(plane.getZ() / 100) << " (FL)\n";
                break;
            }
        }
        
        requestedPlaneId = -1;
    }

    std::cout << "--------------------------------------------------\n";
}

void DataDisplay::requestAugmentedInfo(int planeId) {
    requestedPlaneId = planeId;
}