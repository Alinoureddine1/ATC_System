#include "ComputerSystem.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <sys/neutrino.h>
#include <errno.h>
#include "commandCodes.h"
#include "utils.h"

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
    : chid(-1), operatorChid(-1), displayChid(-1), loggerChid(-1),
      predictionTime(predTime),
      MIN_VERTICAL_SEPARATION(1000.0),
      MIN_HORIZONTAL_SEPARATION(3000.0),
      congestionDegreeSeconds(120)
{
}

void ComputerSystem::run() {
    
    chid = ChannelCreate(0);
    if (chid == -1) {
        std::cerr << "ComputerSystem: ChannelCreate fail: " << strerror(errno) << "\n";
        return;
    }

   
    createPeriodicTasks();

   
    listen();
}

void ComputerSystem::createPeriodicTasks() {
    
    struct {
        int code;
        int interval;
    } tasks[] = {
        { AIRSPACE_VIOLATION_CONSTRAINT_TIMER, 1 }, // check collision
        { OPERATOR_COMMAND_CHECK_TIMER,         1 }, // read console
        { LOG_AIRSPACE_TO_CONSOLE_TIMER,        5 }, // display partial info
        { LOG_AIRSPACE_TO_FILE_TIMER,           20}, // log to file
        { LOG_AIRSPACE_TO_LOGGER_TIMER,         20}  // send to AirspaceLogger
    };
    int nTasks = sizeof(tasks)/sizeof(tasks[0]);

    int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        std::cerr << "ComputerSystem: ConnectAttach fail.\n";
        return;
    }

    // for each periodic task, create a timer
    for (int i = 0; i < nTasks; i++) {
        struct sigevent sev;
        SIGEV_PULSE_INIT(&sev, coid, SIGEV_PULSE_PRIO_INHERIT, tasks[i].code, 0);
        timer_t tid;
        if (timer_create(CLOCK_MONOTONIC, &sev, &tid) == -1) {
            std::cerr << "ComputerSystem: timer_create fail.\n";
            continue;
        }
        // set intervals
        struct itimerspec its;
        its.it_value.tv_sec = tasks[i].interval;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = tasks[i].interval;
        its.it_interval.tv_nsec = 0;
        timer_settime(tid, 0, &its, NULL);
    }
}

void ComputerSystem::listen() {
    struct {
        _pulse hdr;    
        int command; 
    } msg;

    int rcvid;
    double currentTime = 0.0;

    while (true) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
        if (rcvid == 0) {
            
            handlePulse(msg.hdr.code, currentTime);
        } else if (rcvid > 0) {
            
            switch (msg.command) {
                case COMMAND_EXIT_THREAD:
                    MsgReply(rcvid, EOK, NULL, 0);
                    return;
                default:
                    
                    std::cerr << "ComputerSystem: unknown message command=" << msg.command << "\n";
                    MsgError(rcvid, ENOSYS);
                    break;
            }
        } else {
            std::cerr << "ComputerSystem: MsgReceive fail.\n";
        }
    }
}

void ComputerSystem::handlePulse(int code, double &currentTime) {
    
    currentTime += 1.0;

    switch (code) {
        case AIRSPACE_VIOLATION_CONSTRAINT_TIMER:
            violationCheck();
            break;
        case OPERATOR_COMMAND_CHECK_TIMER:
            opConCheck();
            break;
        case LOG_AIRSPACE_TO_CONSOLE_TIMER:
            logSystem(false);
            break;
        case LOG_AIRSPACE_TO_FILE_TIMER:
            logSystem(true);
            break;
        case LOG_AIRSPACE_TO_LOGGER_TIMER: {
            int coid = ConnectAttach(0, 0, loggerChid, _NTO_SIDE_CHANNEL, 0);
            if (coid != -1) {
                AirspaceLogMessage logMsg;
                logMsg.commandType = COMMAND_LOG_AIRSPACE;
                logMsg.timestamp = currentTime;

                int fd = shm_open(SHM_RADAR_DATA, O_RDONLY, 0666);
                if (fd != -1) {
                    RadarData rd;
                    if (read(fd, &rd, sizeof(rd))>0) {
                        for (int i=0; i<rd.numPlanes; i++) {
                            logMsg.positions.push_back(rd.positions[i]);
                            logMsg.velocities.push_back(rd.velocities[i]);
                        }
                    }
                    close(fd);
                }
                MsgSend(coid, &logMsg, sizeof(logMsg), NULL, 0);
                ConnectDetach(coid);
            }
            break;
        }
        default:
            std::cerr << "ComputerSystem: unknown pulse code=" << code << "\n";
            break;
    }
}

bool ComputerSystem::checkSeparation(const Position &p1, const Position &p2) const {
    double dx = fabs(p1.x - p2.x);
    double dy = fabs(p1.y - p2.y);
    double dz = fabs(p1.z - p2.z);
    // True if they are safely separated
    return (dz >= MIN_VERTICAL_SEPARATION &&
            sqrt(dx*dx + dy*dy) >= MIN_HORIZONTAL_SEPARATION);
}

bool ComputerSystem::predictSeparation(const Position &pos1, const Velocity &vel1,
                                       const Position &pos2, const Velocity &vel2) const {
    double px1 = pos1.x + vel1.vx * predictionTime;
    double py1 = pos1.y + vel1.vy * predictionTime;
    double pz1 = pos1.z + vel1.vz * predictionTime;

    double px2 = pos2.x + vel2.vx * predictionTime;
    double py2 = pos2.y + vel2.vy * predictionTime;
    double pz2 = pos2.z + vel2.vz * predictionTime;

    Position fut1 = { pos1.planeId, px1, py1, pz1, time(NULL) };
    Position fut2 = { pos2.planeId, px2, py2, pz2, time(NULL) };
    return checkSeparation(fut1, fut2);
}

void ComputerSystem::violationCheck() {
    // read from /shm_radar_data
    int rfd = shm_open(SHM_RADAR_DATA, O_RDONLY, 0666);
    if (rfd == -1) return;
    RadarData rd;
    if (read(rfd, &rd, sizeof(rd)) == -1) {
        close(rfd);
        return;
    }
    close(rfd);

    positionsSnapshot.clear();
    velocitiesSnapshot.clear();
    for (int i=0; i<rd.numPlanes; i++){
        positionsSnapshot.push_back(rd.positions[i]);
        velocitiesSnapshot.push_back(rd.velocities[i]);
    }

    int n=(int)positionsSnapshot.size();
    for (int i=0; i<n; i++){
        for (int j=i+1; j<n; j++){
            checkForFutureViolation(
                positionsSnapshot[i], velocitiesSnapshot[i],
                positionsSnapshot[j], velocitiesSnapshot[j],
                positionsSnapshot[i].planeId,
                positionsSnapshot[j].planeId
            );
        }
    }
}

void ComputerSystem::checkForFutureViolation(const Position &pos1, const Velocity &vel1,
                                             const Position &pos2, const Velocity &vel2,
                                             int plane1, int plane2)
{
    // project positions after 'congestionDegreeSeconds'
    Vec3 v1 = { vel1.vx, vel1.vy, vel1.vz };
    Vec3 v2 = { vel2.vx, vel2.vy, vel2.vz };

    Vec3 p1 = { pos1.x, pos1.y, pos1.z };
    p1 = p1.sum( v1.scalarMultiplication(congestionDegreeSeconds) );

    Vec3 p2 = { pos2.x, pos2.y, pos2.z };
    p2 = p2.sum( v2.scalarMultiplication(congestionDegreeSeconds) );

    double dx = fabs(p1.x - p2.x);
    double dy = fabs(p1.y - p2.y);
    double dz = fabs(p1.z - p2.z);

    // if they come within 1000 vertically and 3000 horizontally
    if (dz < 1000.0 && sqrt(dx*dx + dy*dy) < 3000.0) {
        int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
        if (coid != -1){
            OperatorConsoleCommandMessage alert;
            alert.systemCommandType = OPCON_CONSOLE_COMMAND_ALERT;
            alert.plane1=plane1;
            alert.plane2=plane2;
            alert.collisionTimeSeconds=(double)congestionDegreeSeconds;
            OperatorConsoleResponseMessage r;
            MsgSend(coid, &alert, sizeof(alert), &r, sizeof(r));
            ConnectDetach(coid);
        }
    }
}

void ComputerSystem::opConCheck() {
    // Ask OperatorConsole for the top user command
    int coid = ConnectAttach(0, 0, operatorChid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) return;

    OperatorConsoleCommandMessage sendMsg;
    sendMsg.systemCommandType=OPCON_CONSOLE_COMMAND_GET_USER_COMMAND;
    OperatorConsoleResponseMessage rcvMsg;

    if (MsgSend(coid, &sendMsg, sizeof(sendMsg), &rcvMsg, sizeof(rcvMsg)) == -1) {
        std::cout << "ComputerSystem: can't get user request from OperatorConsole\n";
        ConnectDetach(coid);
        return;
    }
    ConnectDetach(coid);

    switch(rcvMsg.userCommandType){
        case OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE:
            break;
        case OPCON_USER_COMMAND_DISPLAY_PLANE_INFO:
            sendDisplayCommand(rcvMsg.planeNumber);
            break;
        case OPCON_USER_COMMAND_UPDATE_CONGESTION:
            congestionDegreeSeconds=(int)rcvMsg.newCongestionValue;
            std::cout<<"ComputerSystem: updated congestion="<<congestionDegreeSeconds<<"\n";
            break;
        case OPCON_USER_COMMAND_SET_PLANE_VELOCITY:
            sendVelocityUpdateToComm(rcvMsg.planeNumber, rcvMsg.newVelocity);
            break;
    }
}

void ComputerSystem::logSystem(bool toFile) {
    // read plane states
    int rfd = shm_open(SHM_RADAR_DATA, O_RDONLY, 0666);
    if (rfd == -1) return;
    RadarData rd;
    if (read(rfd, &rd, sizeof(rd))==-1){
        close(rfd);
        return;
    }
    close(rfd);

    // build dataDisplayCommandMessage
    dataDisplayCommandMessage msg;
    msg.commandType = (toFile) ? COMMAND_LOG : COMMAND_GRID;

    std::vector<int> ids;
    std::vector<Vec3> poss, vels;
    for (int i=0; i<rd.numPlanes; i++){
        ids.push_back(rd.positions[i].planeId);
        Vec3 p = {rd.positions[i].x, rd.positions[i].y, rd.positions[i].z};
        Vec3 v = {rd.velocities[i].vx, rd.velocities[i].vy, rd.velocities[i].vz};
        poss.push_back(p);
        vels.push_back(v);
    }
    size_t n=ids.size();
    msg.commandBody.multiple.numberOfAircrafts=n;
    msg.commandBody.multiple.planeIDArray  = new int[n];
    msg.commandBody.multiple.positionArray = new Vec3[n];
    msg.commandBody.multiple.velocityArray = new Vec3[n];
    for (size_t i=0; i<n; i++){
        msg.commandBody.multiple.planeIDArray[i]  = ids[i];
        msg.commandBody.multiple.positionArray[i] = poss[i];
        msg.commandBody.multiple.velocityArray[i] = vels[i];
    }

    // send to DataDisplay
    int coid = ConnectAttach(0, 0, displayChid, _NTO_SIDE_CHANNEL, 0);
    if (coid!=-1){
        MsgSend(coid, &msg, sizeof(msg), NULL, 0);
        ConnectDetach(coid);
    }

    delete[] msg.commandBody.multiple.planeIDArray;
    delete[] msg.commandBody.multiple.positionArray;
    delete[] msg.commandBody.multiple.velocityArray;
}

void ComputerSystem::sendDisplayCommand(int planeNumber){
    // read plane from RadarData
    int rfd=shm_open(SHM_RADAR_DATA, O_RDONLY, 0666);
    if (rfd==-1) {
        std::cout<<"ComputerSystem: can't open SHM_RADAR_DATA\n";
        return;
    }
    RadarData rd;
    if (read(rfd,&rd,sizeof(rd))==-1){
        close(rfd);
        return;
    }
    close(rfd);

    dataDisplayCommandMessage msg;
    msg.commandType=COMMAND_ONE_PLANE;
    msg.commandBody.one.aircraftID=planeNumber;
    msg.commandBody.one.position={0,0,0};
    msg.commandBody.one.velocity={0,0,0};

    bool found=false;
    for (int i=0; i<rd.numPlanes; i++){
        if (rd.positions[i].planeId==planeNumber){
            found=true;
            msg.commandBody.one.position={rd.positions[i].x,rd.positions[i].y,rd.positions[i].z};
            msg.commandBody.one.velocity={rd.velocities[i].vx,rd.velocities[i].vy,rd.velocities[i].vz};
            break;
        }
    }
    if(!found){
        std::cout<<"ComputerSystem: plane "<<planeNumber<<" not found.\n";
        return;
    }
    int coid = ConnectAttach(0,0,displayChid,_NTO_SIDE_CHANNEL,0);
    if(coid!=-1){
        MsgSend(coid,&msg,sizeof(msg),NULL,0);
        ConnectDetach(coid);
    }
}

void ComputerSystem::sendVelocityUpdateToComm(int planeNumber, Vec3 newVel){
    // push command into /shm_commands
    int shm_fd=shm_open(SHM_COMMANDS,O_RDWR,0666);
    if(shm_fd==-1){
        std::cerr<<"ComputerSystem: can't open SHM_COMMANDS\n";
        return;
    }
    CommandQueue* cq = static_cast<CommandQueue*>(
        mmap(nullptr,sizeof(CommandQueue),PROT_READ|PROT_WRITE,MAP_SHARED,shm_fd,0)
    );
    if(cq==MAP_FAILED){
        std::cerr<<"ComputerSystem: can't mmap SHM_COMMANDS\n";
        close(shm_fd);
        return;
    }

    int next = (cq->tail+1) % MAX_COMMANDS;
    if(next==cq->head){
        std::cout<<"ComputerSystem: command queue full.\n";
    } else {
        Command cmd;
        cmd.planeId=planeNumber;
        cmd.code=CMD_VELOCITY;
        cmd.value[0]=newVel.x;
        cmd.value[1]=newVel.y;
        cmd.value[2]=newVel.z;
        cmd.timestamp=time(nullptr);

        cq->commands[cq->tail] = cmd;
        cq->tail=next;
        std::cout<<"ComputerSystem: queued velocity update to plane "<<planeNumber<<"\n";
    }

    munmap(cq,sizeof(CommandQueue));
    close(shm_fd);
}

void ComputerSystem::update(double currentTime){
    (void)currentTime;
}

void* ComputerSystem::start(void* context){
    auto cs=static_cast<ComputerSystem*>(context);
    cs->run();
    return nullptr;
}
