#ifndef OPERATOR_CONSOLE_H
#define OPERATOR_CONSOLE_H

#include <vector>
#include <string>
#include "Plane.h"
#include "commandCodes.h"

class OperatorConsole {
private:
    std::vector<Plane>& planes; 
    std::string commandLogPath; 
    void logCommand(const std::string& command);

public:
    OperatorConsole(std::vector<Plane>& planes, const std::string& logPath = "/tmp/logs/commandlog.txt");
    void sendCommand(const Command& command);
    void requestPlaneInfo(int planeId);
    void displayMenu();
    bool processInput();
};

#endif // OPERATOR_CONSOLE_H