#ifndef AIRSPACE_LOGGER_H
#define AIRSPACE_LOGGER_H

#include <string>
#include <vector>
#include "commandCodes.h"

/**
 * AirspaceLogger receives COMMAND_LOG_AIRSPACE messages from ComputerSystem,
 * writes them to a file for record.
 */
class AirspaceLogger {
    private:
        std::string logPath;
        int chid;
        
        void logAirspaceState(const std::vector<Position>& positions,
                              const std::vector<Velocity>& velocities,
                              double timestamp);
                              
        void registerChannelId();
    
    public:
        AirspaceLogger(const std::string& logPath = DEFAULT_AIRSPACE_LOG_PATH);
        void run();
        
        int getChid() const { return chid; }
        
        static void* start(void* context);
    };

#endif // AIRSPACE_LOGGER_H
