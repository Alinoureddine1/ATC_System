#ifndef COMMUNICATION_SYSTEM_H
#define COMMUNICATION_SYSTEM_H

#include <string>
#include "commandCodes.h"

/**
 * CommunicationSystem reads commands from /shm_commands,
 * simulates sending them to planes (i.e. prints to stdout/log).
 */
class CommunicationSystem {
private:
    std::string transmissionLogPath;
    void logTransmission(const std::string& message);

public:
    CommunicationSystem(const std::string& logPath = "/data/home/qnxuser/logs/transmissionlog.txt");
    void send(int planeId, const Command& command);

    
    void run();
};

#endif // COMMUNICATION_SYSTEM_H
