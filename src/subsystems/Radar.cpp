#include "Radar.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include "commandCodes.h"
#include "utils.h"

Radar::Radar() {}

void Radar::detectAircraft(std::vector<Plane>& planes) {
    trackedPlanes.clear();
    for (auto &p : planes) {
        trackedPlanes.push_back(&p);
    }
}

void Radar::update(double currentTime) {
    // update planes
    for (size_t i = 0; i < trackedPlanes.size(); /* no increment here */) {
        Plane* plane = trackedPlanes[i];
        plane->updatePosition(currentTime);
        
        // Check if plane has left airspace (completely)
        if (!isPositionWithinBounds(plane->getX(), plane->getY(), plane->getZ())) {
            std::cout << printTimeStamp() << " [Radar] Plane " << plane->getId() 
                      << " has left the airspace.\n";
            trackedPlanes.erase(trackedPlanes.begin() + i);
        } else {
            i++; // Only increment if we didn't remove a plane
        }
    }

    // write states to shared memory
    RadarData data;
    data.numPlanes = (int)trackedPlanes.size();
    for (int i=0; i<data.numPlanes && i<MAX_PLANES; i++) {
        Plane* p = trackedPlanes[i];
        data.positions[i] = { p->getId(), p->getX(), p->getY(), p->getZ(), time(nullptr) };
        data.velocities[i] = { p->getId(), p->getVx(), p->getVy(), p->getVz(), time(nullptr) };
    }

    int shm_fd = shm_open(SHM_RADAR_DATA, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Radar: cannot open " << SHM_RADAR_DATA << "\n";
        return;
    }
    ftruncate(shm_fd, sizeof(data));

    void* ptr = mmap(nullptr, sizeof(data), PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Radar: mmap fail.\n";
        close(shm_fd);
        return;
    }
    memcpy(ptr, &data, sizeof(data));
    munmap(ptr, sizeof(data));
    close(shm_fd);
}