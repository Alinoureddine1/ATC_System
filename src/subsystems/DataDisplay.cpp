#include "DataDisplay.h"
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <sys/neutrino.h>
#include "utils.h"

DataDisplay::DataDisplay()
    : chid(-1), fd(-1)
{
}

int DataDisplay::getChid() const {
    return chid;
}

void DataDisplay::run() {
    chid = ChannelCreate(0);
    if (chid == -1) {
        std::cerr << "DataDisplay: ChannelCreate fail.\n";
        return;
    }
    fd = open("/data/home/qnxuser/airspacelog.txt", O_CREAT|O_WRONLY|O_APPEND, 0666);
    if (fd == -1) {
        std::cerr << "DataDisplay: cannot open log.\n";
    }

    receiveMessage();

    if (fd != -1) close(fd);
    ChannelDestroy(chid);
}

void DataDisplay::receiveMessage() {
    dataDisplayCommandMessage msg;
    int rcvid;
    while (true) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            std::cerr << "DataDisplay: MsgReceive fail.\n";
            continue;
        }
        switch (msg.commandType) {
            case COMMAND_ONE_PLANE: {
                MsgReply(rcvid, EOK, nullptr, 0);
                std::cout << printTimeStamp() << " [DataDisplay] Single plane "
                          << msg.commandBody.one.aircraftID
                          << " pos(" << msg.commandBody.one.position.x << ","
                                     << msg.commandBody.one.position.y << ","
                                     << msg.commandBody.one.position.z << ")"
                          << " vel(" << msg.commandBody.one.velocity.x << ","
                                     << msg.commandBody.one.velocity.y << ","
                                     << msg.commandBody.one.velocity.z << ")\n";
                break;
            }
            case COMMAND_MULTIPLE_PLANE: {
                MsgReply(rcvid, EOK, nullptr, 0);
                for (size_t i=0; i<msg.commandBody.multiple.numberOfAircrafts; i++) {
                    int pid = msg.commandBody.multiple.planeIDArray[i];
                    Vec3 pp = msg.commandBody.multiple.positionArray[i];
                    Vec3 vv = msg.commandBody.multiple.velocityArray[i];
                    std::cout << printTimeStamp() << " [DataDisplay] Plane " << pid
                              << " pos(" << pp.x << "," << pp.y << "," << pp.z << ")"
                              << " vel(" << vv.x << "," << vv.y << "," << vv.z << ")\n";
                }
                break;
            }
            case COMMAND_GRID: {
                std::string g = generateGrid(msg.commandBody.multiple);
                MsgReply(rcvid, EOK, nullptr, 0);
                std::cout << printTimeStamp() << " [DataDisplay] GRID:\n" << g << "\n";
                break;
            }
            case COMMAND_LOG: {
                std::string g = generateGrid(msg.commandBody.multiple);
                MsgReply(rcvid, EOK, nullptr, 0);
                std::cout << printTimeStamp() << " [DataDisplay] LOG:\n" << g << "\n";
                if (fd != -1) {
                    std::string s = printTimeStamp() + " LOG:\n" + g + "\n";
                    write(fd, s.c_str(), s.size());
                }
                break;
            }
            case COMMAND_EXIT_THREAD:
                MsgReply(rcvid, EOK, nullptr, 0);
                return;
            default:
                std::cerr << "DataDisplay: unknown commandType=" << msg.commandType << "\n";
                MsgError(rcvid, ENOSYS);
                break;
        }
    }
}

std::string DataDisplay::generateGrid(const multipleAircraftDisplay& airspaceInfo) {
    constexpr int rowSize = 25;
    constexpr int colSize = 25;
    constexpr int cell = 4000;

    std::string grid[rowSize][colSize];
    for (size_t i=0; i<airspaceInfo.numberOfAircrafts; i++) {
        double px = airspaceInfo.positionArray[i].x;
        double py = airspaceInfo.positionArray[i].y;
        int planeId = airspaceInfo.planeIDArray[i];

        for (int r=0; r<rowSize; r++) {
            double yMin = cell*r, yMax = cell*(r+1);
            if (py>=yMin && py<yMax) {
                for (int c=0; c<colSize; c++) {
                    double xMin = cell*c, xMax=cell*(c+1);
                    if (px>=xMin && px<xMax) {
                        if (!grid[r][c].empty()) grid[r][c]+=",";
                        grid[r][c]+=std::to_string(planeId);
                    }
                }
            }
        }
    }

    std::stringstream out;
    for (int r=0; r<rowSize; r++) {
        out << "\n";
        for (int c=0; c<colSize; c++) {
            if (grid[r][c].empty()) out<<"| ";
            else out<<"|"<<grid[r][c];
        }
    }
    out<<"\n";
    return out.str();
}

void DataDisplay::displayAirspace(double currentTime, const std::vector<Position>& positions) {
    std::cout << printTimeStamp() << " DataDisplay: airspace t=" << currentTime << "s\n";
    for (auto &p : positions) {
        std::cout << "  plane " << p.planeId << " => ("
                  << p.x << "," << p.y << "," << p.z << ")\n";
    }
}

void DataDisplay::requestAugmentedInfo(int planeId) {
    std::cout << "[DataDisplay] requestAugmentedInfo(plane=" << planeId << ")\n";
}

void* DataDisplay::start(void* context) {
    auto dd = static_cast<DataDisplay*>(context);
    dd->run();
    return nullptr;
}
