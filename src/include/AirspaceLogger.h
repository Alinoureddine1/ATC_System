#ifndef AIRSPACE_LOGGER_H
#define AIRSPACE_LOGGER_H

#include <vector>
#include "Radar.h"
#include "commandCodes.h"

class AirspaceLogger {
private:
    Radar& radar; 
    std::string logPath; 
    void logAirspaceState(const std::vector<Position>& positions, const std::vector<Velocity>& velocities, double timestamp);

public:
    AirspaceLogger(Radar& radar, const std::string& logPath = "/tmp/logs/historylog.txt");
    void update(double currentTime);
};

#endif // AIRSPACE_LOGGER_H