#ifndef OPERATOR_CONSOLE_H
#define OPERATOR_CONSOLE_H

#include <string>
#include <queue>
#include <pthread.h>
#include <vector>
#include "commandCodes.h"

/**
 * OperatorConsole: reads user commands from stdin, logs them,
 * and sends them to ComputerSystem by queueing responses or pulses.
 */
class OperatorConsole {
private:
    std::string commandLogPath;
    int chid;

    static pthread_mutex_t mutex;
    static std::queue<OperatorConsoleResponseMessage> responseQueue;

    void logCommand(const std::string& cmd);
    void listen();
    static void* cinRead(void* param);
    static void tokenize(std::vector<std::string>& dest, std::string& str);
    
    // Initialize channel ID in shared memory
    void registerChannelId();

public:
    OperatorConsole(const std::string& logPath = DEFAULT_COMMAND_LOG_PATH);
    int  getChid() const;
    void run();
    void displayMenu();
    bool processInput();

    void sendCommand(const Command& command);
    void requestPlaneInfo(int planeId);

    static void* start(void* context);
};

#endif // OPERATOR_CONSOLE_H