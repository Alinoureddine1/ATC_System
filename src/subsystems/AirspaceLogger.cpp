#include "AirspaceLogger.h"
#include <iostream>
#include <stdio.h>
#include <sys/neutrino.h>
#include "utils.h"

AirspaceLogger::AirspaceLogger(const std::string& lp)
    : logPath(lp), chid(-1)
{
}

void AirspaceLogger::logAirspaceState(const std::vector<Position>& positions,
                                      const std::vector<Velocity>& velocities,
                                      double timestamp)
{
    FILE* fp = fopen(logPath.c_str(), "a");
    if (!fp) {
        std::cerr << "AirspaceLogger: cannot open " << logPath << "\n";
        return;
    }
    fprintf(fp, "%s Logging at t=%.1f\n", printTimeStamp().c_str(), timestamp);
    for (size_t i=0; i<positions.size(); i++) {
        fprintf(fp, " Plane %d => (%.1f,%.1f,%.1f) / vel(%.1f,%.1f,%.1f)\n",
                positions[i].planeId,
                positions[i].x, positions[i].y, positions[i].z,
                velocities[i].vx, velocities[i].vy, velocities[i].vz);
    }
    fprintf(fp, "------------------------\n");
    fclose(fp);
}

void AirspaceLogger::run() {
    chid = ChannelCreate(0);
    if (chid==-1) {
        std::cerr<<"AirspaceLogger: ChannelCreate fail.\n";
        return;
    }

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
