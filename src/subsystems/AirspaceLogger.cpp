#include "AirspaceLogger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils.h"

AirspaceLogger::AirspaceLogger(Radar& radar, const std::string& logPath)
    : radar(radar), logPath(logPath) {}

void AirspaceLogger::logAirspaceState(const std::vector<Position>& positions, 
                                      const std::vector<Velocity>& velocities, double timestamp) {
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << printTimeStamp() << " Timestamp: " << timestamp << "s\n";
        for (size_t i = 0; i < positions.size(); ++i) {
            logFile << "Plane " << positions[i].planeId << ": "
                    << "Position=(" << positions[i].x << ", " << positions[i].y << ", " << positions[i].z << "), "
                    << "Velocity=(" << velocities[i].vx << ", " << velocities[i].vy << ", " << velocities[i].vz << ")\n";
        }
        logFile << "------------------------\n";
        logFile.close();
    } else {
        std::cerr << printTimeStamp() << " Error opening history log file: " << logPath << "\n";
    }
}

void AirspaceLogger::update(double currentTime) {
    // Log airspace state every 20 seconds
    if (static_cast<int>(currentTime) % 20 == 0 && currentTime > 0) {
        auto positions = radar.getPositions();
        auto velocities = radar.getVelocities();
        if (positions.size() == velocities.size()) {
            logAirspaceState(positions, velocities, currentTime);
            std::cout << printTimeStamp() << " Airspace history logged at " << currentTime << "s\n";
        } else {
            std::cerr << printTimeStamp() << " Error: Mismatch between positions and velocities\n";
        }
    }
}