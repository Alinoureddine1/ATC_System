#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "commandCodes.h"
#include "utils.h"

static volatile sig_atomic_t running=1;
static void handleSig(int){running=0;}

int main(int argc, char* argv[]) {
    std::cout << printTimeStamp() << " ATCController starting\n";

    // create radar data shm
    int shmRadarFd = shm_open(SHM_RADAR_DATA, O_CREAT|O_RDWR, 0666);
    ftruncate(shmRadarFd, sizeof(RadarData));
    RadarData* radarData = (RadarData*)mmap(nullptr, sizeof(RadarData),
        PROT_READ|PROT_WRITE, MAP_SHARED, shmRadarFd, 0);
    radarData->numPlanes=0;

    // create commands shm
    int shmCmdFd = shm_open(SHM_COMMANDS, O_CREAT|O_RDWR, 0666);
    ftruncate(shmCmdFd, sizeof(CommandQueue));
    CommandQueue* cmdQueue = (CommandQueue*)mmap(nullptr, sizeof(CommandQueue),
        PROT_READ|PROT_WRITE, MAP_SHARED, shmCmdFd, 0);
    cmdQueue->head=0;
    cmdQueue->tail=0;

    // fork each subsystem
    std::string basePath="/tmp/atc";

    pid_t p1=fork();    
    if (p1==0){
        execl((basePath+"/Radar").c_str(),"Radar", (char*)nullptr);
        _exit(1);
    }
    pid_t p2=fork();
    if (p2==0){
        execl((basePath+"/ComputerSystem").c_str(),"ComputerSystem",(char*)nullptr);
        _exit(1);
    }
    pid_t p3=fork();
    if (p3==0){
        execl((basePath+"/DataDisplay").c_str(),"DataDisplay",(char*)nullptr);
        _exit(1);
    }
    pid_t p4=fork();
    if (p4==0){
        execl((basePath+"/CommunicationSystem").c_str(),"CommunicationSystem",(char*)nullptr);
        _exit(1);
    }
    pid_t p5=fork();
    if (p5==0){
        execl((basePath+"/OperatorConsole").c_str(),"OperatorConsole",(char*)nullptr);
        _exit(1);
    }
    pid_t p6=fork();
    if (p6==0){
        execl((basePath+"/AirspaceLogger").c_str(),"AirspaceLogger",(char*)nullptr);
        _exit(1);
    }

    signal(SIGINT, handleSig);
    while(running) pause();

    // cleanup
    shm_unlink(SHM_RADAR_DATA);
    shm_unlink(SHM_COMMANDS);
    munmap(radarData, sizeof(RadarData));
    munmap(cmdQueue, sizeof(CommandQueue));
    close(shmRadarFd);
    close(shmCmdFd);

    std::cout << printTimeStamp() << " ATCController shutting down\n";
    return 0;
}
