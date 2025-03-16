#include "Radar.h"
#include <iostream>

Radar::Radar() : trackedPlanes() {}

void Radar::detectAircraft(std::vector<Plane>& planes) {
    // Simulate PSR: detect all planes in the airspace
    trackedPlanes.clear();
    for (auto& plane : planes) {
        trackedPlanes.push_back(&plane);
        std::cout << printTimeStamp() << " Radar detected Plane " << plane.getId() 
                  << " at (" << plane.getX() << ", " << plane.getY() << ", " << plane.getZ() << ")\n";
    }
}

void Radar::interrogateAircraft() {
    // Simulate SSR: interrogate detected planes
    for (auto* plane : trackedPlanes) {
        if (plane) {
            std::cout << printTimeStamp() << " Radar interrogated Plane " << plane->getId() 
                      << ": Position (" << plane->getX() << ", " << plane->getY() << ", " << plane->getZ() 
                      << "), Velocity (" << plane->getVx() << ", " << plane->getVy() << ", " << plane->getVz() << ")\n";
        }
    }
}

void Radar::update(std::vector<Plane>& planes, double currentTime) {
    detectAircraft(planes);
    interrogateAircraft();
}

std::vector<Position> Radar::getPositions() const {
    std::vector<Position> positions;
    for (auto* plane : trackedPlanes) {
        if (plane) {
            Position pos;
            pos.planeId = plane->getId();
            pos.x = plane->getX();
            pos.y = plane->getY();
            pos.z = plane->getZ();
            pos.timestamp = time(nullptr); 
            positions.push_back(pos);
        }
    }
    return positions;
}

std::vector<Velocity> Radar::getVelocities() const {
    std::vector<Velocity> velocities;
    for (auto* plane : trackedPlanes) {
        if (plane) {
            Velocity vel;
            vel.planeId = plane->getId();
            vel.vx = plane->getVx();
            vel.vy = plane->getVy();
            vel.vz = plane->getVz();
            vel.timestamp = time(nullptr); 
            velocities.push_back(vel);
        }
    }
    return velocities;
}