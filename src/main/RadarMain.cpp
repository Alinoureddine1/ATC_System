#include "Radar.h"
#include "Plane.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h> // For sleep function (also contains pause)
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>
#include <signal.h>
#include <thread>
#include <memory> // For std::unique_ptr

// Signal handler for clean shutdown
static volatile sig_atomic_t running = 1;
static void handleSig(int) { running = 0; }

/**
 * Parse the input file containing plane data
 * Format: time id x y z speedX speedY speedZ
 * Each line represents a plane that will enter the airspace at the specified time
 */
std::map<int, std::vector<std::unique_ptr<Plane>>> parseInputFile(const std::string& filename) {
    std::map<int, std::vector<std::unique_ptr<Plane>>> timeToPlanes;
    std::ifstream infile(filename);
    
    if (!infile.is_open()) {
        logRadarMessage("Warning: Could not open input file " + filename, LOG_WARNING);
        logRadarMessage("Using default plane configuration");
        return timeToPlanes;
    }

    std::string line;
    
    // Read header line (if exists)
    std::getline(infile, line);
    
    bool hasHeader = false;
    {
        std::istringstream testIss(line);
        int time, id;
        if (!(testIss >> time >> id)) {
            hasHeader = true;
        }
    }
    
    if (!hasHeader) {
        infile.clear();
        infile.seekg(0, std::ios::beg);
    }
    
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        int time, id;
        double x, y, z, speedX, speedY, speedZ;
        
        if (!(iss >> time >> id >> x >> y >> z >> speedX >> speedY >> speedZ)) {
            logRadarMessage("Warning: Invalid line in input file: " + line, LOG_WARNING);
            continue;
        }
        
        // Verify coordinates are within airspace
        if (!isPositionWithinBounds(x, y, z)) {
            logRadarMessage("Warning: Initial position for plane " + std::to_string(id) + 
                          " is outside airspace, adjusting: (" + 
                          std::to_string(x) + "," + 
                          std::to_string(y) + "," + 
                          std::to_string(z) + ")", LOG_WARNING);
            
            // Clamp to airspace boundaries
            if (x < AIRSPACE_X_MIN) x = AIRSPACE_X_MIN;
            else if (x > AIRSPACE_X_MAX) x = AIRSPACE_X_MAX;
            if (y < AIRSPACE_Y_MIN) y = AIRSPACE_Y_MIN;
            else if (y > AIRSPACE_Y_MAX) y = AIRSPACE_Y_MAX;
            if (z < AIRSPACE_Z_MIN) z = AIRSPACE_Z_MIN;
            else if (z > AIRSPACE_Z_MAX) z = AIRSPACE_Z_MAX;
        }
        
        // Create a new Plane and add it to the appropriate time bucket
        timeToPlanes[time].push_back(std::unique_ptr<Plane>(new Plane(id, x, y, z, speedX, speedY, speedZ)));
    }
    
    logRadarMessage("Successfully parsed input file with " + 
                  std::to_string(timeToPlanes.size()) + " time entries");
    return timeToPlanes;
}

void runRadarSystem() {
    // Vector of plane pointers
    std::vector<std::unique_ptr<Plane>> activePlanes;
    
    // Add default planes
    activePlanes.push_back(std::unique_ptr<Plane>(new Plane(1, 10000.0, 20000.0, 5000.0, 100.0, 50.0, 0.0)));
    activePlanes.push_back(std::unique_ptr<Plane>(new Plane(2, 30000.0, 40000.0, 7000.0, -50.0, 100.0, 0.0)));
    
    // Create directory for input file if it doesn't exist
    mkdir("/tmp/atc", 0777);
    
    // Parse input file (if exists)
    std::string inputFilePath = DEFAULT_PLANE_INPUT_PATH;
    auto timeToPlanes = parseInputFile(inputFilePath);
    
    // Add any planes that should appear at time 0
    if (timeToPlanes.find(0) != timeToPlanes.end()) {
        for (auto& plane : timeToPlanes[0]) {
            // Check if a plane with this ID already exists
            bool exists = false;
            for (const auto& existingPlane : activePlanes) {
                if (existingPlane->getId() == plane->getId()) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                int planeId = plane->getId();
                activePlanes.push_back(std::move(plane));
                logRadarMessage("Added plane ID " + std::to_string(planeId) + " at time 0");
            }
        }
    }
    
    // Create radar and provide pointers to planes
    Radar radar;
    std::vector<Plane*> planePointers;
    for (const auto& plane : activePlanes) {
        planePointers.push_back(plane.get());
    }
    radar.detectAircraft(planePointers, 0.0);
    
    double t = 0.0;
    while (running) {
        // Check if any new planes should be added at this time
        int currentTimeInt = static_cast<int>(t);
        if (timeToPlanes.find(currentTimeInt) != timeToPlanes.end()) {
            for (auto& plane : timeToPlanes[currentTimeInt]) {
                bool exists = false;
                int planeId = plane->getId();
                
                for (const auto& p : activePlanes) {
                    if (p->getId() == planeId) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    activePlanes.push_back(std::move(plane));
                    logRadarMessage("Added plane ID " + std::to_string(planeId) + 
                                  " at time " + std::to_string(currentTimeInt));
                }
            }
            
            // Update radar with new planes
            planePointers.clear();
            for (const auto& plane : activePlanes) {
                planePointers.push_back(plane.get());
            }
            radar.detectAircraft(planePointers, t);
        }
        
        radar.update(t);
        sleep(1);
        t += 1.0;
    }
}

int main() {
    // Set up signal handler for graceful termination
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logRadarMessage("Subsystem starting");
    
    // Start the radar system in a thread so we can handle signals
    std::thread radarThread(runRadarSystem);
    
    // Wait for termination signal
    while (running) {
        sleep(1); // Use sleep(1) instead of pause() for better portability
    }
    
    logRadarMessage("Shutdown signal received", LOG_WARNING);
    
    // Wait for radar thread to complete
    if (radarThread.joinable()) {
        radarThread.join();
    }
    
    logRadarMessage("Shutdown complete");
    return 0;
}