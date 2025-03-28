#ifndef RADAR_H
#define RADAR_H

#include <vector>
#include <mutex>
#include "Plane.h"

/**
 * Radar updates plane positions every second and writes to /shm_radar_data.
 */
class Radar {
private:
    std::vector<Plane*> trackedPlanes;
    std::mutex planesMutex; 

public:
    Radar();
    ~Radar();
    
    void detectAircraft(std::vector<Plane*>& planes, double currentTime);
    
    void update(double currentTime);
    
    void addPlane(Plane* plane, double currentTime);
    void removePlane(int planeId);
};

#endif // RADAR_H