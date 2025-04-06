#include "Radar.h"
#include "Plane.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h> 
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>
#include <signal.h>
#include <thread>
#include <memory> 

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
        
        timeToPlanes[time].push_back(std::unique_ptr<Plane>(new Plane(id, x, y, z, speedX, speedY, speedZ)));
    }
    
    logRadarMessage("Successfully parsed input file with " + 
                  std::to_string(timeToPlanes.size()) + " time entries");
    return timeToPlanes;
}

void runRadarSystem() {
    mkdir("/tmp/atc", 0777);

    std::string inputFilePath = DEFAULT_PLANE_INPUT_PATH;
    
    auto timeToPlanes = parseInputFile(inputFilePath);

    bool usingCustomPlanes = false;
    for (const auto &kv : timeToPlanes) {
        if (!kv.second.empty()) {
            usingCustomPlanes = true;
            break;
        }
    }

    std::vector<std::unique_ptr<Plane>> activePlanes;
    if (!usingCustomPlanes) {
        logRadarMessage("No custom plane data found; using default planes");
        activePlanes.push_back(std::unique_ptr<Plane>(new Plane(
            1, 10000.0, 20000.0, 5000.0, 100.0, 50.0, 0.0)));
        activePlanes.push_back(std::unique_ptr<Plane>(new Plane(
            2, 30000.0, 40000.0, 7000.0, -50.0, 100.0, 0.0)));
    } else {
        logRadarMessage("Custom plane file found; skipping default planes");
    }

    if (timeToPlanes.find(0) != timeToPlanes.end()) {
        for (auto &planePtr : timeToPlanes[0]) {
            bool exists = false;
            for (const auto &existingPlane : activePlanes) {
                if (existingPlane->getId() == planePtr->getId()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                int pid = planePtr->getId();
                activePlanes.push_back(std::move(planePtr));
                logRadarMessage("Added plane ID " + std::to_string(pid) + " at time 0");
            }
        }
        timeToPlanes.erase(0);
    }

    Radar radar;
    std::vector<Plane*> planePointers;
    planePointers.reserve(activePlanes.size());
    for (auto &planeUptr : activePlanes) {
        planePointers.push_back(planeUptr.get());
    }
    radar.detectAircraft(planePointers, 0.0);

    double t = 0.0;
    while (running) {
        int currentTimeInt = static_cast<int>(t);
        if (timeToPlanes.find(currentTimeInt) != timeToPlanes.end()) {
            for (auto &planePtr : timeToPlanes[currentTimeInt]) {
                bool exists = false;
                for (const auto &p : activePlanes) {
                    if (p->getId() == planePtr->getId()) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    activePlanes.push_back(std::move(planePtr));
                    logRadarMessage("Added plane ID " +
                                    std::to_string(activePlanes.back()->getId()) +
                                    " at time " + std::to_string(currentTimeInt));
                }
            }
            timeToPlanes.erase(currentTimeInt);

            planePointers.clear();
            planePointers.reserve(activePlanes.size());
            for (auto &planeUptr : activePlanes) {
                planePointers.push_back(planeUptr.get());
            }
            radar.detectAircraft(planePointers, t);
        }

        radar.update(t);
        sleep(1);
        t += 1.0;
    }
}

int main() {
    signal(SIGINT, handleSig);
    signal(SIGTERM, handleSig);
    
    logRadarMessage("Subsystem starting");
    
    std::thread radarThread(runRadarSystem);
    
    while (running) {
        sleep(1); 
    }
    
    logRadarMessage("Shutdown signal received", LOG_WARNING);
    
    if (radarThread.joinable()) {
        radarThread.join();
    }
    
    logRadarMessage("Shutdown complete");
    return 0;
}