#ifndef DATA_DISPLAY_H
#define DATA_DISPLAY_H

#include <vector>
#include "ComputerSystem.h"
#include "commandCodes.h"

class DataDisplay {
private:
    ComputerSystem& computerSystem; 
    const std::vector<Plane>& planes; 
    int requestedPlaneId; 

public:
    DataDisplay(ComputerSystem& computerSystem, const std::vector<Plane>& planes);
    void displayAirspace(double currentTime);
    void requestAugmentedInfo(int planeId);
};

#endif // DATA_DISPLAY_H