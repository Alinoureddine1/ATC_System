#ifndef RADAR_H
#define RADAR_H

#include <vector>
#include "Plane.h"
#include "commandCodes.h"

class Radar {
private:
    std::vector<Plane*> trackedPlanes; // Pointers to planes for simulation

public:
    Radar();
    void detectAircraft(std::vector<Plane>& planes); // Simulate PSR
    void interrogateAircraft(); // Simulate SSR
    void update(std::vector<Plane>& planes, double currentTime);
    std::vector<Position> getPositions() const;
    std::vector<Velocity> getVelocities() const;
};

#endif // RADAR_H