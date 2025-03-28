#ifndef COMPUTER_SYSTEM_H
#define COMPUTER_SYSTEM_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include "commandCodes.h"

/**
 * ComputerSystem reads plane data from /shm_radar_data,
 * checks for separation, logs to DataDisplay & AirspaceLogger,
 * and processes OperatorConsole commands.
 */
class ComputerSystem
{
private:
    int chid;
    int operatorChid;
    pid_t operatorPid;
    int displayChid;
    pid_t displayPid; 
    int loggerChid;
    pid_t loggerPid; 

    double predictionTime;
    const double MIN_VERTICAL_SEPARATION;
    const double MIN_HORIZONTAL_SEPARATION;
    int congestionDegreeSeconds;

    std::atomic<bool> violationCheckInProgress;
    std::atomic<bool> logInProgress;
    std::mutex eventMutex;
    std::condition_variable eventCV;
    bool emergencyEvent;

    std::vector<Position> positionsSnapshot;
    std::vector<Velocity> velocitiesSnapshot;

    void createPeriodicTasks();

    void listen();

    void handlePulse(int code, double &currentTime);

    bool initializeChannelIds();

    void registerChannelId();

    void logSystem(bool toFile);
    void opConCheck();
    void sendDisplayCommand(int planeNumber);
    void sendVelocityUpdateToComm(int planeNumber, Vec3 newVelocity);
    void sendLogToAirspaceLogger(double currentTime);

    bool checkSeparation(const Position &pos1, const Position &pos2) const;
    bool predictSeparation(const Position &pos1, const Velocity &vel1,
                           const Position &pos2, const Velocity &vel2) const;

    void violationCheck();
    void checkForFutureViolation(const Position &pos1, const Velocity &vel1,
                                 const Position &pos2, const Velocity &vel2,
                                 int plane1, int plane2);

    double calculateTimeToMinimumDistance(const Position &pos1, const Velocity &vel1,
                                          const Position &pos2, const Velocity &vel2);
    void processEmergencyEvents();
    void triggerEmergencyEvent();

public:
    ComputerSystem(double predictionTime = 120.0);
    void run();

    // Getters and setters
    int getChid() const { return chid; }
    void setOperatorChid(int id) { operatorChid = id; }
    void setDisplayChid(int id) { displayChid = id; }
    void setLoggerChid(int id) { loggerChid = id; }

    void update(double currentTime);

    static void *start(void *context);

    std::vector<Position> getDisplayData() const { return positionsSnapshot; }
};

#endif // COMPUTER_SYSTEM_H