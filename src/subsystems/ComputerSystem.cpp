#include "ComputerSystem.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <sys/neutrino.h>
#include <thread> 
#include <errno.h>
#include "commandCodes.h"
#include "utils.h"
#include "shm_utils.h"

/**
 * The ComputerSystem sets up multiple periodic tasks:
 *   1) violationCheck() every 1s
 *   2) opConCheck() every 1s
 *   3) logSystem(false) every 5s  (to DataDisplay)
 *   4) logSystem(true) every 20s  (to DataDisplay, but with COMMAND_LOG)
 *   5) LOG_AIRSPACE_TO_LOGGER_TIMER every 20s to send a message to AirspaceLogger
 *
 * We read plane data from /shm_radar_data each time we do violationCheck() or logging.
 * If OperatorConsole sets plane velocities, we queue them in /shm_commands for CommunicationSystem.
 */

 ComputerSystem::ComputerSystem(double predTime)
 : chid(-1), 
   operatorChid(-1), 
   displayChid(-1), 
   loggerChid(-1),
   predictionTime(predTime),
   MIN_VERTICAL_SEPARATION(1000.0),
   MIN_HORIZONTAL_SEPARATION(3000.0),
   congestionDegreeSeconds(120),
   violationCheckInProgress(false),
   logInProgress(false),
   emergencyEvent(false)
{
 logComputerSystemMessage("ComputerSystem initialized with prediction time: " + 
                        std::to_string(predTime) + "s");
}

bool ComputerSystem::initializeChannelIds()
{
    logComputerSystemMessage("Initializing channel IDs");
    int retries = 0;
    const int MAX_RETRIES = 30;  // 30 for now to avoid infinite loop
    bool initialized = false;

    while (retries < MAX_RETRIES && !initialized)
    {
        bool success = accessSharedMemory<ChannelIds>(
            SHM_CHANNELS,
            sizeof(ChannelIds),
            O_RDWR,
            false, 
            [this, &initialized](ChannelIds* channels) {
                // Check that all channel IDs are initialized and unique
                if (channels->operatorChid > 0 &&
                    channels->displayChid > 0 &&
                    channels->loggerChid > 0 &&
                    channels->operatorChid != channels->displayChid &&
                    channels->operatorChid != channels->loggerChid &&
                    channels->displayChid != channels->loggerChid)
                {
                    operatorChid = OPERATOR_CONSOLE_CHANNEL_ID;
                    displayChid = DATA_DISPLAY_CHANNEL_ID;
                    loggerChid = AIRSPACE_LOGGER_CHANNEL_ID;

                    channels->computerChid = COMPUTER_SYSTEM_CHANNEL_ID;
                    initialized = true;
                    
                    logComputerSystemMessage("Successfully initialized channel IDs: " +
                                           std::string("operator=") + std::to_string(operatorChid) + ", " +
                                           std::string("display=") + std::to_string(displayChid) + ", " +
                                           std::string("logger=") + std::to_string(loggerChid));
                }
                else
                {
                    logComputerSystemMessage("Waiting for unique subsystem IDs..." + 
                                           std::string(" operator=") + std::to_string(channels->operatorChid) + 
                                           std::string(" display=") + std::to_string(channels->displayChid) + 
                                           std::string(" logger=") + std::to_string(channels->loggerChid), LOG_INFO);
                }
            }
        );

        if (!success || !initialized)
        {
            sleep(1);
            retries++;
        }
    }

    if (!initialized) {
        logComputerSystemMessage("Failed to initialize channel IDs after " + 
                               std::to_string(MAX_RETRIES) + " attempts", LOG_ERROR);
    }

    return initialized;
}

void ComputerSystem::run() {
    chid = ChannelCreate(COMPUTER_SYSTEM_CHANNEL_ID);
    if (chid == -1) {
        logComputerSystemMessage("ChannelCreate failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    logComputerSystemMessage("Channel created with ID: " + std::to_string(chid));
    
    if (!initializeChannelIds()) {
        logComputerSystemMessage("Failed to initialize channel IDs", LOG_ERROR);
        return;
    }
    
    logComputerSystemMessage("All channel IDs initialized. Ready to create periodic tasks.");
    
    // Start emergency event processing thread
    std::thread eventThread(&ComputerSystem::processEmergencyEvents, this);
    eventThread.detach();
    
    createPeriodicTasks();
    listen();
}

void ComputerSystem::createPeriodicTasks()
{
    struct
    {
        int code;
        int interval;
        std::string description;
    } tasks[] = {
        {AIRSPACE_VIOLATION_CONSTRAINT_TIMER, 1, "Airspace violation check"},
        {OPERATOR_COMMAND_CHECK_TIMER, 1, "Operator command check"},
        {LOG_AIRSPACE_TO_CONSOLE_TIMER, 5, "Log airspace to console"},
        {LOG_AIRSPACE_TO_FILE_TIMER, 20, "Log airspace to file"},
        {LOG_AIRSPACE_TO_LOGGER_TIMER, 20, "Send log to AirspaceLogger"}
    };
    int nTasks = sizeof(tasks) / sizeof(tasks[0]);

    int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1)
    {
        logComputerSystemMessage("ConnectAttach failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    // For each periodic task, create a timer
    for (int i = 0; i < nTasks; i++)
    {
        struct sigevent sev;
        SIGEV_PULSE_INIT(&sev, coid, SIGEV_PULSE_PRIO_INHERIT, tasks[i].code, 0);
        timer_t tid;
        if (timer_create(CLOCK_MONOTONIC, &sev, &tid) == -1)
        {
            logComputerSystemMessage("Failed to create timer for " + 
                                   tasks[i].description + ": " + 
                                   std::string(strerror(errno)), LOG_ERROR);
            continue;
        }
        
        // Set intervals
        struct itimerspec its;
        its.it_value.tv_sec = tasks[i].interval;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = tasks[i].interval;
        its.it_interval.tv_nsec = 0;
        
        if (timer_settime(tid, 0, &its, NULL) == -1) {
            logComputerSystemMessage("Failed to set timer for " + 
                                   tasks[i].description + ": " + 
                                   std::string(strerror(errno)), LOG_ERROR);
        } else {
            logComputerSystemMessage("Created periodic task: " + 
                                   tasks[i].description + " every " + 
                                   std::to_string(tasks[i].interval) + "s");
        }
    }
}

void ComputerSystem::listen()
{
    struct
    {
        _pulse hdr;
        int command;
    } msg;

    int rcvid;
    double currentTime = 0.0;

    logComputerSystemMessage("Starting message processing loop");

    while (true)
    {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
        if (rcvid == 0)
        {
            // Pulse received
            handlePulse(msg.hdr.code, currentTime);
        }
        else if (rcvid > 0)
        {
            // Message received
            switch (msg.command)
            {
            case COMMAND_EXIT_THREAD:
                logComputerSystemMessage("Received exit command");
                MsgReply(rcvid, EOK, NULL, 0);
                return;
            default:
                logComputerSystemMessage("Unknown message command: " + 
                                       std::to_string(msg.command), LOG_WARNING);
                MsgError(rcvid, ENOSYS);
                break;
            }
        }
        else
        {
            logComputerSystemMessage("MsgReceive failed: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
        }
    }
}

void ComputerSystem::handlePulse(int code, double &currentTime)
{
    currentTime += 1.0;

    switch (code)
    {
    case AIRSPACE_VIOLATION_CONSTRAINT_TIMER:
        if (!violationCheckInProgress.exchange(true))
        {
            violationCheck();
            violationCheckInProgress = false;
        }
        else
        {
            logComputerSystemMessage("Skipping violation check, previous check still in progress", LOG_DEBUG);
        }
        break;

    case OPERATOR_COMMAND_CHECK_TIMER:
        opConCheck();
        break;

    case LOG_AIRSPACE_TO_CONSOLE_TIMER:
    case LOG_AIRSPACE_TO_FILE_TIMER:
        if (!logInProgress.exchange(true))
        {
            logSystem(code == LOG_AIRSPACE_TO_FILE_TIMER);
            logInProgress = false;
        }
        else
        {
            logComputerSystemMessage("Skipping log operation, previous log still in progress", LOG_DEBUG);
        }
        break;

    case LOG_AIRSPACE_TO_LOGGER_TIMER:
        sendLogToAirspaceLogger(currentTime);
        break;

    default:
        logComputerSystemMessage("Unknown pulse code: " + std::to_string(code), LOG_WARNING);
        break;
    }
}

void ComputerSystem::sendLogToAirspaceLogger(double currentTime)
{
    // If logger channel not initialized, try to get it from shared memory
    if (loggerChid == -1)
    {
        bool success = accessSharedMemory<ChannelIds>(
            SHM_CHANNELS,
            sizeof(ChannelIds),
            O_RDONLY,
            false,
            [this](ChannelIds* channels) {
                loggerChid = channels->loggerChid;
            }
        );

        if (!success || loggerChid == -1)
        {
            logComputerSystemMessage("AirspaceLogger not ready yet, skipping log", LOG_WARNING);
            return;
        }
    }

    int coid = ConnectAttach(0, 0, loggerChid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1)
    {
        logComputerSystemMessage("Failed to connect to AirspaceLogger: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    AirspaceLogMessage logMsg;
    logMsg.commandType = COMMAND_LOG_AIRSPACE;
    logMsg.timestamp = currentTime;

    // Get radar data
    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [&logMsg](RadarData* rd) {
            for (int i = 0; i < rd->numPlanes; i++)
            {
                logMsg.positions.push_back(rd->positions[i]);
                logMsg.velocities.push_back(rd->velocities[i]);
            }
        }
    );

    if (success)
    {
        if (MsgSend(coid, &logMsg, sizeof(logMsg), NULL, 0) == -1)
        {
            logComputerSystemMessage("Failed to send log to AirspaceLogger: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
        }
        else
        {
            logComputerSystemMessage("Sent airspace log to AirspaceLogger");
        }
    }
    
    ConnectDetach(coid);
}

double ComputerSystem::calculateTimeToMinimumDistance(const Position &pos1, const Velocity &vel1,
                                                    const Position &pos2, const Velocity &vel2)
{
    // Calculate relative velocity
    double dvx = vel1.vx - vel2.vx;
    double dvy = vel1.vy - vel2.vy;
    double dvz = vel1.vz - vel2.vz;

    // Calculate relative position
    double dx = pos1.x - pos2.x;
    double dy = pos1.y - pos2.y;
    double dz = pos1.z - pos2.z;

    // Calculate time to minimum distance (first derivative of distance function set to zero)
    double t = 0;
    double denom = dvx * dvx + dvy * dvy + dvz * dvz;

    if (denom > 0.001)
    { // Avoid division by near-zero
        t = -(dx * dvx + dy * dvy + dz * dvz) / denom;
    }

    // If time is negative, closest approach already happened
    return (t > 0) ? t : 0;
}

bool ComputerSystem::checkSeparation(const Position &p1, const Position &p2) const
{
    double dx = fabs(p1.x - p2.x);
    double dy = fabs(p1.y - p2.y);
    double dz = fabs(p1.z - p2.z);
    
    double horizontalSep = sqrt(dx * dx + dy * dy);
    
    // True if they are safely separated
    return (dz >= MIN_VERTICAL_SEPARATION || 
            horizontalSep >= MIN_HORIZONTAL_SEPARATION);
}

bool ComputerSystem::predictSeparation(const Position &pos1, const Velocity &vel1,
                                     const Position &pos2, const Velocity &vel2) const
{
    // Calculate future positions
    double px1 = pos1.x + vel1.vx * predictionTime;
    double py1 = pos1.y + vel1.vy * predictionTime;
    double pz1 = pos1.z + vel1.vz * predictionTime;

    double px2 = pos2.x + vel2.vx * predictionTime;
    double py2 = pos2.y + vel2.vy * predictionTime;
    double pz2 = pos2.z + vel2.vz * predictionTime;

    // Create position objects for future positions
    Position fut1 = {pos1.planeId, px1, py1, pz1, time(NULL)};
    Position fut2 = {pos2.planeId, px2, py2, pz2, time(NULL)};
    
    // Check separation at future time
    return checkSeparation(fut1, fut2);
}

void ComputerSystem::violationCheck()
{
    // Read current radar data
    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [this](RadarData* rd) {
            // Update our snapshots
            positionsSnapshot.clear();
            velocitiesSnapshot.clear();
            
            for (int i = 0; i < rd->numPlanes; i++)
            {positionsSnapshot.push_back(rd->positions[i]);
                velocitiesSnapshot.push_back(rd->velocities[i]);
            }
        }
    );

    if (!success) {
        logComputerSystemMessage("Failed to read radar data for violation check", LOG_ERROR);
        return;
    }

    // Check each pair of planes for potential violations
    int n = static_cast<int>(positionsSnapshot.size());
    for (int i = 0; i < n; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            checkForFutureViolation(
                positionsSnapshot[i], velocitiesSnapshot[i],
                positionsSnapshot[j], velocitiesSnapshot[j],
                positionsSnapshot[i].planeId,
                positionsSnapshot[j].planeId);
        }
    }
}

void ComputerSystem::checkForFutureViolation(const Position &pos1, const Velocity &vel1,
                                           const Position &pos2, const Velocity &vel2,
                                           int plane1, int plane2)
{
    // Calculate time to closest approach
    double timeToClosestApproach = calculateTimeToMinimumDistance(pos1, vel1, pos2, vel2);

    // If closest approach is within our prediction window
    if (timeToClosestApproach <= congestionDegreeSeconds)
    {
        // Calculate positions at closest approach
        Vec3 p1 = {pos1.x, pos1.y, pos1.z};
        p1 = p1.sum(Vec3{vel1.vx, vel1.vy, vel1.vz}.scalarMultiplication(timeToClosestApproach));

        Vec3 p2 = {pos2.x, pos2.y, pos2.z};
        p2 = p2.sum(Vec3{vel2.vx, vel2.vy, vel2.vz}.scalarMultiplication(timeToClosestApproach));

        // Check separation at closest approach
        double dx = fabs(p1.x - p2.x);
        double dy = fabs(p1.y - p2.y);
        double dz = fabs(p1.z - p2.z);

        double horizontalSeparation = sqrt(dx * dx + dy * dy);

        // Check if they violate separation
        if (dz < MIN_VERTICAL_SEPARATION && horizontalSeparation < MIN_HORIZONTAL_SEPARATION)
        {
            // Alert the operator console
            int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
            if (coid != -1)
            {
                OperatorConsoleCommandMessage alert;
                alert.systemCommandType = OPCON_CONSOLE_COMMAND_ALERT;
                alert.plane1 = plane1;
                alert.plane2 = plane2;
                alert.collisionTimeSeconds = timeToClosestApproach;
                OperatorConsoleResponseMessage r;
                
                if (MsgSend(coid, &alert, sizeof(alert), &r, sizeof(r)) == -1) {
                    logComputerSystemMessage("Failed to send violation alert to operator: " + 
                                           std::string(strerror(errno)), LOG_ERROR);
                }
                
                ConnectDetach(coid);

                logComputerSystemMessage("ALERT: Planes " + std::to_string(plane1) + 
                                       " and " + std::to_string(plane2) +
                                       " will violate separation in " + 
                                       std::to_string(timeToClosestApproach) + " seconds", LOG_WARNING);
            }
        }
    }
}

void ComputerSystem::opConCheck()
{
    // Ask OperatorConsole for the top user command
    int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        logComputerSystemMessage("Failed to connect to OperatorConsole: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    OperatorConsoleCommandMessage sendMsg;
    sendMsg.systemCommandType = OPCON_CONSOLE_COMMAND_GET_USER_COMMAND;
    OperatorConsoleResponseMessage rcvMsg;

    if (MsgSend(coid, &sendMsg, sizeof(sendMsg), &rcvMsg, sizeof(rcvMsg)) == -1)
    {
        logComputerSystemMessage("Failed to get user command from OperatorConsole: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        ConnectDetach(coid);
        return;
    }
    ConnectDetach(coid);

    // Process the response from OperatorConsole
    switch (rcvMsg.userCommandType)
    {
    case OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE:
        // No command to process
        break;
        
    case OPCON_USER_COMMAND_DISPLAY_PLANE_INFO:
        logComputerSystemMessage("Received request to display plane " + 
                               std::to_string(rcvMsg.planeNumber));
        sendDisplayCommand(rcvMsg.planeNumber);
        break;
        
    case OPCON_USER_COMMAND_UPDATE_CONGESTION:
        congestionDegreeSeconds = static_cast<int>(rcvMsg.newCongestionValue);
        logComputerSystemMessage("Updated congestion parameter to " + 
                               std::to_string(congestionDegreeSeconds) + " seconds");
        break;
        
    case OPCON_USER_COMMAND_SET_PLANE_VELOCITY:
        logComputerSystemMessage("Sending velocity update for plane " + 
                               std::to_string(rcvMsg.planeNumber));
        sendVelocityUpdateToComm(rcvMsg.planeNumber, rcvMsg.newVelocity);
        break;
        
    default:
        logComputerSystemMessage("Unknown user command type: " + 
                               std::to_string(rcvMsg.userCommandType), LOG_WARNING);
        break;
    }
}

void ComputerSystem::logSystem(bool toFile) {
    // Read the current plane data
    std::vector<int> ids;
    std::vector<Vec3> poss, vels;
    
    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [&ids, &poss, &vels](RadarData* rd) {
            for (int i = 0; i < rd->numPlanes; i++) {
                // Only include valid data
                if (rd->positions[i].planeId >= 0) {
                    ids.push_back(rd->positions[i].planeId);
                    Vec3 p = {rd->positions[i].x, rd->positions[i].y, rd->positions[i].z};
                    Vec3 v = {rd->velocities[i].vx, rd->velocities[i].vy, rd->velocities[i].vz};
                    poss.push_back(p);
                    vels.push_back(v);
                }
            }
        }
    );
    
    if (!success || ids.empty()) {
        logComputerSystemMessage("No valid radar data for logging", LOG_WARNING);
        return;
    }
    
    // Safer message creation for DataDisplay
    dataDisplayCommandMessage msg;
    msg.commandType = (toFile) ? COMMAND_LOG : COMMAND_GRID;
    
    size_t n = ids.size();
    
    // Allocate memory for arrays that will be sent in the message
    int* planeIDArray = new (std::nothrow) int[n];
    Vec3* positionArray = new (std::nothrow) Vec3[n];
    Vec3* velocityArray = new (std::nothrow) Vec3[n];
    
    if (!planeIDArray || !positionArray || !velocityArray) {
        logComputerSystemMessage("Failed to allocate memory for display data", LOG_ERROR);
        
        // Clean up any allocated memory
        if (planeIDArray) delete[] planeIDArray;
        if (positionArray) delete[] positionArray;
        if (velocityArray) delete[] velocityArray;
        
        return;
    }
    
    // Copy data to arrays
    for (size_t i = 0; i < n; i++) {
        planeIDArray[i] = ids[i];
        positionArray[i] = poss[i];
        velocityArray[i] = vels[i];
    }
    
    msg.commandBody.multiple.numberOfAircrafts = n;
    msg.commandBody.multiple.planeIDArray = planeIDArray;
    msg.commandBody.multiple.positionArray = positionArray;
    msg.commandBody.multiple.velocityArray = velocityArray;
    
    // Send message to DataDisplay
    int coid = ConnectAttach(0, 0, displayChid, _NTO_SIDE_CHANNEL, 0);
    if (coid != -1) {
        if (MsgSend(coid, &msg, sizeof(msg), NULL, 0) == -1) {
            logComputerSystemMessage("Failed to send log data to DataDisplay: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
        } else {
            logComputerSystemMessage("Sent " + std::string(toFile ? "log" : "grid") + 
                                   " data to DataDisplay with " + std::to_string(n) + " planes");
        }
        ConnectDetach(coid);
    } else {
        logComputerSystemMessage("Failed to connect to DataDisplay: " + 
                               std::string(strerror(errno)), LOG_ERROR);
    }
    
    // Clean up allocated memory
    delete[] planeIDArray;
    delete[] positionArray;
    delete[] velocityArray;
}

void ComputerSystem::processEmergencyEvents() {
    logComputerSystemMessage("Emergency event processing thread started");
    
    while (true) {
        // Use a mutex and condition variable to wait for emergency events
        std::unique_lock<std::mutex> lock(eventMutex);
        eventCV.wait_for(lock, std::chrono::milliseconds(100), [this]{ return emergencyEvent; });
        
        if (emergencyEvent) {
            emergencyEvent = false;
            
            // Process immediate safety concerns
            violationCheck();
            
            // Signal operator console about emergency
            int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
            if (coid != -1) {
                OperatorConsoleCommandMessage alert;
                alert.systemCommandType = OPCON_CONSOLE_COMMAND_ALERT;
                alert.plane1 = -1; // Special value to indicate system-wide emergency
                alert.plane2 = -1;
                alert.collisionTimeSeconds = 0; // Immediate
                OperatorConsoleResponseMessage r;
                
                if (MsgSend(coid, &alert, sizeof(alert), &r, sizeof(r)) == -1) {
                    logComputerSystemMessage("Failed to send emergency alert: " + 
                                           std::string(strerror(errno)), LOG_ERROR);
                } else {
                    logComputerSystemMessage("Emergency event processed and alert sent to operator");
                }
                
                ConnectDetach(coid);
            }
        }
    }
}

void ComputerSystem::triggerEmergencyEvent() {
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        emergencyEvent = true;
    }
    eventCV.notify_one();
    logComputerSystemMessage("Emergency event triggered", LOG_WARNING);
}

void ComputerSystem::sendDisplayCommand(int planeNumber)
{
    // Find the plane in radar data
    bool found = false;
    dataDisplayCommandMessage msg;
    msg.commandType = COMMAND_ONE_PLANE;
    msg.commandBody.one.aircraftID = planeNumber;
    msg.commandBody.one.position = {0, 0, 0};
    msg.commandBody.one.velocity = {0, 0, 0};

    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [&found, &msg, planeNumber](RadarData* rd) {
            for (int i = 0; i < rd->numPlanes; i++) {
                if (rd->positions[i].planeId == planeNumber) {
                    found = true;
                    msg.commandBody.one.position = {
                        rd->positions[i].x, 
                        rd->positions[i].y, 
                        rd->positions[i].z
                    };
                    msg.commandBody.one.velocity = {
                        rd->velocities[i].vx, 
                        rd->velocities[i].vy, 
                        rd->velocities[i].vz
                    };
                    break;
                }
            }
        }
    );

    if (!success || !found) {
        logComputerSystemMessage("Plane " + std::to_string(planeNumber) + " not found", LOG_WARNING);
        return;
    }

    // Send to DataDisplay
    int coid = ConnectAttach(0, 0, displayChid, _NTO_SIDE_CHANNEL, 0);
    if (coid != -1) {
        if (MsgSend(coid, &msg, sizeof(msg), NULL, 0) == -1) {
            logComputerSystemMessage("Failed to send plane info to DataDisplay: " + 
                                   std::string(strerror(errno)), LOG_ERROR);
        } else {
            logComputerSystemMessage("Sent info for plane " + std::to_string(planeNumber) + 
                                   " to DataDisplay");
        }
        ConnectDetach(coid);
    } else {
        logComputerSystemMessage("Failed to connect to DataDisplay: " + 
                               std::string(strerror(errno)), LOG_ERROR);
    }
}

void ComputerSystem::sendVelocityUpdateToComm(int planeNumber, Vec3 newVel)
{
    // Queue command for CommunicationSystem
    bool success = accessSharedMemory<CommandQueue>(
        SHM_COMMANDS,
        sizeof(CommandQueue),
        O_RDWR,
        false,
        [this, planeNumber, newVel](CommandQueue* cq) {
            int next = (cq->tail + 1) % MAX_COMMANDS;
            if (next == cq->head) {
                logComputerSystemMessage("Command queue full, cannot send velocity update", LOG_WARNING);
                return;
            }
            
            Command cmd;
            cmd.planeId = planeNumber;
            cmd.code = CMD_VELOCITY;
            cmd.value[0] = newVel.x;
            cmd.value[1] = newVel.y;
            cmd.value[2] = newVel.z;
            cmd.timestamp = time(nullptr);
            
            cq->commands[cq->tail] = cmd;
            cq->tail = next;
            
            logComputerSystemMessage("Queued velocity update for plane " + 
                                   std::to_string(planeNumber) + ": (" +
                                   std::to_string(newVel.x) + ", " +
                                   std::to_string(newVel.y) + ", " +
                                   std::to_string(newVel.z) + ")");
        }
    );
    
    if (!success) {
        logComputerSystemMessage("Failed to access command queue for velocity update", LOG_ERROR);
    }
}

void ComputerSystem::update(double currentTime)
{
    (void)currentTime;
}

void* ComputerSystem::start(void* context)
{
    auto cs = static_cast<ComputerSystem*>(context);
    cs->run();
    return nullptr;
}
