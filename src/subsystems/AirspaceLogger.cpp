#include "AirspaceLogger.h"
#include <iostream>
#include <stdio.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"

AirspaceLogger::AirspaceLogger(const std::string &lp)
    : logPath(lp), chid(-1)
{
}

void AirspaceLogger::registerChannelId()
{
    int shmChannelsFd = shm_open(SHM_CHANNELS, O_RDWR, 0666);
    if (shmChannelsFd == -1)
    {
        std::cerr << "AirspaceLogger: Cannot open channel IDs shared memory\n";
        return;
    }

    ChannelIds *channels = (ChannelIds *)mmap(nullptr, sizeof(ChannelIds),
                                              PROT_READ | PROT_WRITE, MAP_SHARED, shmChannelsFd, 0);
    if (channels == MAP_FAILED)
    {
        std::cerr << "AirspaceLogger: Cannot map channel IDs shared memory\n";
        close(shmChannelsFd);
        return;
    }

    channels->loggerChid = chid;

    munmap(channels, sizeof(ChannelIds));
    close(shmChannelsFd);

    std::cout << "AirspaceLogger: Registered channel ID " << chid << " in shared memory\n";
}

void AirspaceLogger::logAirspaceState(const std::vector<Position> &positions,
                                      const std::vector<Velocity> &velocities,
                                      double timestamp)
{
    // Ensure directory exists
    mkdir("/tmp/atc/logs", 0777);

    FILE *fp = fopen(logPath.c_str(), "a");
    if (!fp)
    {
        std::cerr << "AirspaceLogger: cannot open " << logPath << "\n";
        return;
    }
    fprintf(fp, "%s Logging at t=%.1f\n", printTimeStamp().c_str(), timestamp);
    for (size_t i = 0; i < positions.size(); i++)
    {
        fprintf(fp, " Plane %d => (%.1f,%.1f,%.1f) / vel(%.1f,%.1f,%.1f)\n",
                positions[i].planeId,
                positions[i].x, positions[i].y, positions[i].z,
                velocities[i].vx, velocities[i].vy, velocities[i].vz);
    }
    fprintf(fp, "------------------------\n");
    fclose(fp);
}

void AirspaceLogger::run() {
    // Ensure log directory exists
    mkdir("/tmp/atc/logs", 0777);
    
    chid = ChannelCreate(0);
    if (chid==-1) {
        std::cerr<<"AirspaceLogger: ChannelCreate fail.\n";
        return;
    }
    
    std::cout << "AirspaceLogger: Channel created with ID: " << chid << std::endl;
    
    registerChannelId();

    AirspaceLogMessage msg;
    while (true) {
        int rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            std::cerr<<"AirspaceLogger: MsgReceive fail.\n";
            continue;
        }
        if (msg.commandType == COMMAND_LOG_AIRSPACE) {
            logAirspaceState(msg.positions, msg.velocities, msg.timestamp);
            MsgReply(rcvid, EOK, nullptr, 0);
        } else if (msg.commandType == COMMAND_EXIT_THREAD) {
            MsgReply(rcvid, EOK, nullptr, 0);
            break;
        }
    }
    ChannelDestroy(chid);
}

void* AirspaceLogger::start(void* context) {
    auto a = static_cast<AirspaceLogger*>(context);
    a->run();
    return nullptr;
}