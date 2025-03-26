#include "Radar.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include "commandCodes.h"
#include "utils.h"
#include "shm_utils.h"

Radar::Radar() {
    logRadarMessage("Radar system initialized");
}

Radar::~Radar() {
    // Only stop tracking planes, don't destroy them as we don't own them
    std::lock_guard<std::mutex> lock(planesMutex);
    for (Plane* plane : trackedPlanes) {
        plane->stop();
    }
    trackedPlanes.clear();
    logRadarMessage("Radar system shutdown, all plane tracking stopped");
}

void Radar::detectAircraft(std::vector<Plane*>& planes, double currentTime) {
    std::lock_guard<std::mutex> lock(planesMutex);
    trackedPlanes.clear();
    
    logRadarMessage("Detecting aircraft, found " + std::to_string(planes.size()));
    
    for (Plane* p : planes) {
        trackedPlanes.push_back(p);
        // Start the plane's thread if it's newly added
        p->start(currentTime);
        
        logRadarMessage("Added plane " + std::to_string(p->getId()) + 
                       " at position (" + 
                       std::to_string(p->getX()) + "," + 
                       std::to_string(p->getY()) + "," + 
                       std::to_string(p->getZ()) + ")");
    }
}

void Radar::addPlane(Plane* plane, double currentTime) {
    std::lock_guard<std::mutex> lock(planesMutex);
    
    // Check if plane already exists
    auto it = std::find_if(trackedPlanes.begin(), trackedPlanes.end(), 
                          [plane](Plane* p) { return p->getId() == plane->getId(); });
    
    if (it == trackedPlanes.end()) {
        trackedPlanes.push_back(plane);
        // Start the plane's thread
        plane->start(currentTime);
        
        logRadarMessage("Started tracking plane " + std::to_string(plane->getId()) + 
                       " at position (" + 
                       std::to_string(plane->getX()) + "," + 
                       std::to_string(plane->getY()) + "," + 
                       std::to_string(plane->getZ()) + ")");
    } else {
        logRadarMessage("Plane " + std::to_string(plane->getId()) + 
                       " already tracked, ignoring add request", LOG_WARNING);
    }
}

void Radar::removePlane(int planeId) {
    std::lock_guard<std::mutex> lock(planesMutex);
    
    auto it = std::find_if(trackedPlanes.begin(), trackedPlanes.end(), 
                          [planeId](Plane* p) { return p->getId() == planeId; });
    
    if (it != trackedPlanes.end()) {
        // No need to stop the thread here, as we don't control the plane object lifecycle
        trackedPlanes.erase(it);
        logRadarMessage("Stopped tracking plane " + std::to_string(planeId));
    } else {
        logRadarMessage("Cannot remove plane " + std::to_string(planeId) + 
                       ", not currently tracked", LOG_WARNING);
    }
}

void Radar::update(double currentTime) {
    std::lock_guard<std::mutex> lock(planesMutex);
    
    // Remove planes that are out of bounds
    for (size_t i = 0; i < trackedPlanes.size(); /* no increment here */) {
        Plane* plane = trackedPlanes[i];
        
        // Check if plane has left airspace
        if (!isPositionWithinBounds(plane->getX(), plane->getY(), plane->getZ())) {
            logRadarMessage("Plane " + std::to_string(plane->getId()) + 
                         " has left the airspace", LOG_WARNING);
            trackedPlanes.erase(trackedPlanes.begin() + i);
        } else {
            i++; // Only increment if we didn't remove a plane
        }
    }

    // Update shared memory with current plane states
    RadarData data;
    data.numPlanes = (int)std::min(trackedPlanes.size(), (size_t)MAX_PLANES);
    
    if (data.numPlanes > 0) {
        logRadarMessage("Updating radar data with " + std::to_string(data.numPlanes) + " planes", LOG_DEBUG);
    }
    
    for (int i = 0; i < data.numPlanes; i++) {
        Plane* p = trackedPlanes[i];
        data.positions[i] = { p->getId(), p->getX(), p->getY(), p->getZ(), time(nullptr) };
        data.velocities[i] = { p->getId(), p->getVx(), p->getVy(), p->getVz(), time(nullptr) };
    }

    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDWR,
        true, // Create if missing
        [&data](RadarData* rd) {
            // Copy the data
            memcpy(rd, &data, sizeof(RadarData));
        }
    );
    
    if (!success) {
        logRadarMessage("Failed to update radar data in shared memory", LOG_ERROR);
    }
}