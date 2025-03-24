#ifndef DATA_DISPLAY_H
#define DATA_DISPLAY_H

#include <vector>
#include <string>
#include "commandCodes.h"

/**
 * DataDisplay listens on a channel for commands (COMMAND_GRID, COMMAND_ONE_PLANE, etc.),
 * prints the results, and can optionally log them to a file.
 */
class DataDisplay {
private:
    int chid;
    int fd;

    void receiveMessage();
    std::string generateGrid(const multipleAircraftDisplay& airspaceInfo);

public:
    DataDisplay();
    int getChid() const;
    void run();
    void displayAirspace(double currentTime, const std::vector<Position>& positions);
    void requestAugmentedInfo(int planeId);

    static void* start(void* context);
};

#endif // DATA_DISPLAY_H
