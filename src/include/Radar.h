#ifndef RADAR_H
#define RADAR_H

#include <vector>
#include "Plane.h"

/**
 * Radar updates plane positions every second and writes to /shm_radar_data.
 */
class Radar {
private:
    std::vector<Plane*> trackedPlanes;

public:
    Radar();
    void detectAircraft(std::vector<Plane>& planes);
    void update(double currentTime);
};

#endif // RADAR_H
