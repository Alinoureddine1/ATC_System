#include "ComputerSystem.h"
#include <iostream>
#include <cmath>

ComputerSystem::ComputerSystem(Radar& radar, double predictionTime)
    : radar(radar), predictionTime(predictionTime) {}

bool ComputerSystem::checkSeparation(const Position& pos1, const Position& pos2) const {
    double dx = pos1.x - pos2.x;
    double dy = pos1.y - pos2.y;
    double horizontalDist = std::sqrt(dx * dx + dy * dy);
    double dz = std::abs(pos1.z - pos2.z);

    return horizontalDist >= MIN_HORIZONTAL_SEPARATION || dz >= MIN_VERTICAL_SEPARATION;
}

bool ComputerSystem::predictSeparation(const Position& pos1, const Velocity& vel1, 
                                       const Position& pos2, const Velocity& vel2) const {
    Position futurePos1 = pos1;
    futurePos1.x += vel1.vx * predictionTime;
    futurePos1.y += vel1.vy * predictionTime;
    futurePos1.z += vel1.vz * predictionTime;

    Position futurePos2 = pos2;
    futurePos2.x += vel2.vx * predictionTime;
    futurePos2.y += vel2.vy * predictionTime;
    futurePos2.z += vel2.vz * predictionTime;

    return checkSeparation(futurePos1, futurePos2);
}

void ComputerSystem::update(double currentTime) {
    auto positions = radar.getPositions();
    auto velocities = radar.getVelocities();

    // Ensure positions and velocities align
    if (positions.size() != velocities.size()) {
        std::cerr << printTimeStamp() << " Error: Mismatch between positions and velocities\n";
        return;
    }

    // Check for current and predicted separation violations
    for (size_t i = 0; i < positions.size(); ++i) {
        for (size_t j = i + 1; j < positions.size(); ++j) {
            if (!checkSeparation(positions[i], positions[j])) {
                std::cout << printTimeStamp() << " Alert: Current separation violation between Plane " 
                          << positions[i].planeId << " and Plane " << positions[j].planeId << "\n";
            }

            
            if (!predictSeparation(positions[i], velocities[i], positions[j], velocities[j])) {
                std::cout << printTimeStamp() << " Alert: Predicted separation violation within " 
                          << predictionTime << "s between Plane " << positions[i].planeId 
                          << " and Plane " << positions[j].planeId << "\n";
            }
        }
    }
}

std::vector<Position> ComputerSystem::getDisplayData() const {
    return radar.getPositions();
}