#include "Radar.h"
#include "Plane.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>

std::map<int, std::vector<Plane>> parseInputFile(const std::string& filename) {
    std::map<int, std::vector<Plane>> timeToPlanes;
    std::ifstream infile(filename);
    
    if (!infile.is_open()) {
        std::cerr << "[Radar] Warning: Could not open input file " << filename << std::endl;
        std::cerr << "[Radar] Using default plane configuration." << std::endl;
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
            std::cerr << "[Radar] Warning: Invalid line in input file: " << line << std::endl;
            continue;
        }
        
        // Verify coordinates are within airspace
        if (!isPositionWithinBounds(x, y, z)) {
            std::cout << "[Radar] Warning: Initial position for plane " << id 
                      << " is outside airspace, adjusting: (" << x << "," << y << "," << z << ")\n";
            
            // Clamp to airspace boundaries
            if (x < AIRSPACE_X_MIN) x = AIRSPACE_X_MIN;
            else if (x > AIRSPACE_X_MAX) x = AIRSPACE_X_MAX;
            if (y < AIRSPACE_Y_MIN) y = AIRSPACE_Y_MIN;
            else if (y > AIRSPACE_Y_MAX) y = AIRSPACE_Y_MAX;
            if (z < AIRSPACE_Z_MIN) z = AIRSPACE_Z_MIN;
            else if (z > AIRSPACE_Z_MAX) z = AIRSPACE_Z_MAX;
        }
        
        timeToPlanes[time].emplace_back(id, x, y, z, speedX, speedY, speedZ);
    }
    
    std::cout << "[Radar] Successfully parsed input file with " 
              << timeToPlanes.size() << " time entries." << std::endl;
    return timeToPlanes;
}

int main() {
    std::cout << "[Radar] subsystem starting\n";
    
    // Default aircraft if no input file is found
    std::vector<Plane> activePlanes = {
        Plane(1, 10000.0, 20000.0, 5000.0, 100.0, 50.0, 0.0),
        Plane(2, 30000.0, 40000.0, 7000.0, -50.0, 100.0, 0.0)
    };
    
    // Create directory for input file if it doesn't exist
    mkdir("/tmp/atc", 0777);
    
    // Parse input file (if exists)
    std::string inputFilePath = DEFAULT_PLANE_INPUT_PATH;
    std::map<int, std::vector<Plane>> timeToPlanes = parseInputFile(inputFilePath);
    
    // Add any planes that should appear at time 0
    if (timeToPlanes.find(0) != timeToPlanes.end()) {
        for (const auto& plane : timeToPlanes[0]) {
            // Check if a plane with this ID already exists
            bool exists = false;
            for (const auto& existingPlane : activePlanes) {
                if (existingPlane.getId() == plane.getId()) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                activePlanes.push_back(plane);
                std::cout << "[Radar] Added plane ID " << plane.getId() << " at time 0\n";
            }
        }
    }
    
    Radar radar;
    radar.detectAircraft(activePlanes);
    
    double t = 0.0;
    while (true) {
        // Check if any new planes should be added at this time
        int currentTimeInt = static_cast<int>(t);
        if (timeToPlanes.find(currentTimeInt) != timeToPlanes.end()) {
            for (const auto& plane : timeToPlanes[currentTimeInt]) {
                bool exists = false;
                for (const auto& p : activePlanes) {
                    if (p.getId() == plane.getId()) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    activePlanes.push_back(plane);
                    std::cout << "[Radar] Added plane ID " << plane.getId() 
                              << " at time " << currentTimeInt << "\n";
                }
            }
            
            // Update radar with new planes
            radar.detectAircraft(activePlanes);
        }
        
        radar.update(t);
        sleep(1);
        t += 1.0;
    }
    
    return 0;
}