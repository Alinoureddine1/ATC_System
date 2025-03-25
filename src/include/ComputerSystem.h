#ifndef COMPUTER_SYSTEM_H
#define COMPUTER_SYSTEM_H

#include <vector>
#include "commandCodes.h"

/**
 * ComputerSystem reads plane data from /shm_radar_data,
 * checks for separation, logs to DataDisplay & AirspaceLogger,
 * and processes OperatorConsole commands.
 */
class ComputerSystem {
private:
    int chid;
    int operatorChid;
    int displayChid;
    int loggerChid;

    double predictionTime;
    const double MIN_VERTICAL_SEPARATION;
    const double MIN_HORIZONTAL_SEPARATION;
    int congestionDegreeSeconds;

    std::vector<Position> positionsSnapshot;
    std::vector<Velocity> velocitiesSnapshot;

    void createPeriodicTasks();

    void listen();

    void handlePulse(int code, double &currentTime);

    bool initializeChannelIds();

    
    void logSystem(bool toFile);
    void opConCheck();
    void sendDisplayCommand(int planeNumber);
    void sendVelocityUpdateToComm(int planeNumber, Vec3 newVelocity);

    bool checkSeparation(const Position& pos1, const Position& pos2) const;
    bool predictSeparation(const Position& pos1, const Velocity& vel1,
                           const Position& pos2, const Velocity& vel2) const;

    void violationCheck(); // checks for collisions
    void checkForFutureViolation(const Position &pos1, const Velocity &vel1,
                                 const Position &pos2, const Velocity &vel2,
                                 int plane1, int plane2);

public:
    ComputerSystem(double predictionTime = 120.0);
    void run();

    // Getters and setters
    int  getChid() const { return chid; }
    void setOperatorChid(int id) { operatorChid = id; }
    void setDisplayChid(int id)  { displayChid  = id; }
    void setLoggerChid(int id)   { loggerChid   = id; }

    void update(double currentTime);

    static void* start(void* context);

    std::vector<Position> getDisplayData() const { return positionsSnapshot; }
};

#endif // COMPUTER_SYSTEM_H