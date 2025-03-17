#ifndef COMMUNICATION_SYSTEM_H
#define COMMUNICATION_SYSTEM_H

#include <vector>
#include <string>
#include "Plane.h"
#include "commandCodes.h"

class CommunicationSystem {
private:
    std::vector<Plane>& planes; 
    std::string transmissionLogPath; 
    void logTransmission(const std::string& message);

public:
    CommunicationSystem(std::vector<Plane>& planes, 
                        const std::string& logPath = "/tmp/logs/transmissionlog.txt");
    void send(int planeId, const Command& command);
};

#endif // COMMUNICATION_SYSTEM_H