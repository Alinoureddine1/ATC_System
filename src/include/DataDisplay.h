#ifndef DATA_DISPLAY_H
#define DATA_DISPLAY_H

#include <vector>
#include <string>
#include "commandCodes.h"

/**
 * DataDisplay listens on a channel for commands (COMMAND_GRID, COMMAND_ONE_PLANE, etc.),
 * prints the results, and logs them to a file.
 */
class DataDisplay {
    private:
        int chid;
        int fd;
        std::string logPath;
    
        void receiveMessage();
        std::string generateGrid(const multipleAircraftDisplay& airspaceInfo);
    
        // Initialize channel ID in shared memory
        void registerChannelId();
    
    public:
        DataDisplay(const std::string& logPath = DEFAULT_AIRSPACE_LOG_PATH);
        int getChid() const;
        void run();
        void displayAirspace(double currentTime, const std::vector<Position>& positions);
        void requestAugmentedInfo(int planeId);
    
        static void* start(void* context);
    };

#endif // DATA_DISPLAY_H
