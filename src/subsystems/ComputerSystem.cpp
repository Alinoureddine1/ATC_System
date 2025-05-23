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
    const int MAX_RETRIES = 30;
    bool initialized = false;

    while (retries < MAX_RETRIES && !initialized)
    {
        bool success = accessSharedMemory<ChannelIds>(
            SHM_CHANNELS,
            sizeof(ChannelIds),
            O_RDONLY,
            false, 
            [this, &initialized](ChannelIds* channels) {
                if (channels->operatorChid > 0 &&
                    channels->displayChid > 0 &&
                    channels->loggerChid > 0)
                {
                    operatorChid = channels->operatorChid;
                    operatorPid = channels->operatorPid;  
                    displayChid = channels->displayChid;
                    displayPid = channels->displayPid;    
                    loggerChid = channels->loggerChid;
                    loggerPid = channels->loggerPid;      
                    
                    initialized = true;
                    
                    logComputerSystemMessage("Successfully initialized channel IDs: " +
                                           std::string("operator=") + std::to_string(operatorChid) + 
                                           ":" + std::to_string(operatorPid) + ", " +
                                           std::string("display=") + std::to_string(displayChid) + 
                                           ":" + std::to_string(displayPid) + ", " +
                                           std::string("logger=") + std::to_string(loggerChid) + 
                                           ":" + std::to_string(loggerPid));
                }
                else
                {
                    logComputerSystemMessage("Waiting for subsystem IDs..." + 
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

void ComputerSystem::registerChannelId()
{
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds* channels) {
            channels->computerChid = chid;
            channels->computerPid = getpid();  
            logComputerSystemMessage("Registered computer system channel ID " + 
                                   std::to_string(chid) + " with PID " +
                                   std::to_string(getpid()) + " in shared memory");
        }
    );
    
    if (!success) {
        logComputerSystemMessage("Failed to register channel ID in shared memory", LOG_ERROR);
    }
}

void ComputerSystem::run() {
    chid = ChannelCreate(0);
    if (chid == -1) {
        logComputerSystemMessage("ChannelCreate failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    logComputerSystemMessage("Channel created with ID: " + std::to_string(chid));
    
    registerChannelId();
    
    if (!initializeChannelIds()) {
        logComputerSystemMessage("Failed to initialize channel IDs", LOG_ERROR);
        return;
    }
    
    logComputerSystemMessage("All channel IDs initialized. Ready to create periodic tasks.");
    
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
            handlePulse(msg.hdr.code, currentTime);
        }
        else if (rcvid > 0)
        {
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
    if (loggerChid == -1 || loggerPid == -1)
    {
        bool success = accessSharedMemory<ChannelIds>(
            SHM_CHANNELS,
            sizeof(ChannelIds),
            O_RDONLY,
            false,
            [this](ChannelIds* channels) {
                loggerChid = channels->loggerChid;
                loggerPid = channels->loggerPid;
            }
        );

        if (!success || loggerChid == -1 || loggerPid == -1)
        {
            logComputerSystemMessage("AirspaceLogger not ready yet, skipping log", LOG_WARNING);
            return;
        }
    }

    int coid = ConnectAttach(0, loggerPid, loggerChid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1)
    {
        logComputerSystemMessage("Failed to connect to AirspaceLogger: " + 
                               std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    AirspaceLogMessage logMsg;
    logMsg.commandType = COMMAND_LOG_AIRSPACE;
    logMsg.timestamp = currentTime;
    logMsg.numPlanes = 0;

    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [&logMsg](RadarData* rd) {
            logMsg.numPlanes = (rd->numPlanes > MAX_PLANES) ? MAX_PLANES : rd->numPlanes;
            for (int i = 0; i < logMsg.numPlanes; i++) {
                logMsg.positions[i] = rd->positions[i];
                logMsg.velocities[i] = rd->velocities[i];
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
            logComputerSystemMessage("Sent airspace log to AirspaceLogger with " + 
                                   std::to_string(logMsg.numPlanes) + " planes");
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
    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [this](RadarData* rd) {
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
    double timeToClosestApproach = calculateTimeToMinimumDistance(pos1, vel1, pos2, vel2);

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
            int coid = ConnectAttach(0, operatorPid, operatorChid, _NTO_SIDE_CHANNEL, 0);
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
    int coid = ConnectAttach(0, operatorPid, operatorChid, _NTO_SIDE_CHANNEL, 0);
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

    switch (rcvMsg.userCommandType)
    {
    case OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE:
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
    
    dataDisplayCommandMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.commandType = (toFile) ? COMMAND_LOG : COMMAND_GRID;
    
    size_t n = ids.size();
    if (n > MAX_PLANES) {
        logComputerSystemMessage("Too many planes for message, truncating to " + 
                               std::to_string(MAX_PLANES), LOG_WARNING);
        n = MAX_PLANES;
    }
    
    msg.commandBody.multiple.numberOfAircrafts = n;
    
    for (size_t i = 0; i < n; i++) {
        msg.commandBody.multiple.planeIDArray[i] = ids[i];
        msg.commandBody.multiple.positionArray[i] = poss[i];
        msg.commandBody.multiple.velocityArray[i] = vels[i];
    }
    
    int coid = ConnectAttach(0, displayPid, displayChid, _NTO_SIDE_CHANNEL, 0);
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
}

void ComputerSystem::processEmergencyEvents() {
    logComputerSystemMessage("Emergency event processing thread started");
    
    while (true) {
        std::unique_lock<std::mutex> lock(eventMutex);
        eventCV.wait_for(lock, std::chrono::milliseconds(100), [this]{ return emergencyEvent; });
        
        if (emergencyEvent) {
            emergencyEvent = false;
            
            violationCheck();
            
            // Signal operator console about emergency
            int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
            if (coid != -1) {
                OperatorConsoleCommandMessage alert;
                alert.systemCommandType = OPCON_CONSOLE_COMMAND_ALERT;
                alert.plane1 = -1; 
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
    struct {
        dataDisplayCommandMessage msg;
    } stackMsg;
    
    memset(&stackMsg, 0, sizeof(stackMsg));
    
    bool found = false;
    stackMsg.msg.commandType = COMMAND_ONE_PLANE;
    stackMsg.msg.commandBody.one.aircraftID = planeNumber;
    
    bool success = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_RDONLY,
        false,
        [&found, &stackMsg, planeNumber](RadarData* rd) {
            for (int i = 0; i < rd->numPlanes; i++) {
                if (rd->positions[i].planeId == planeNumber) {
                    found = true;
                    stackMsg.msg.commandBody.one.position = {
                        rd->positions[i].x, 
                        rd->positions[i].y, 
                        rd->positions[i].z
                    };
                    stackMsg.msg.commandBody.one.velocity = {
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

    // Send to DataDisplay with correct PID
    int coid = ConnectAttach(0, displayPid, displayChid, _NTO_SIDE_CHANNEL, 0);
    if (coid != -1) {
        if (MsgSend(coid, &stackMsg, sizeof(stackMsg), NULL, 0) == -1) {
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
