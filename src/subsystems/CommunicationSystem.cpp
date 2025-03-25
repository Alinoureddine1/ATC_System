#include "CommunicationSystem.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cmath>
#include <sys/stat.h>
#include "utils.h"

CommunicationSystem::CommunicationSystem(const std::string& logPath)
    : transmissionLogPath(logPath)
{
}

void CommunicationSystem::send(int planeId, const Command& command) {
    std::string msg = "CommSystem: direct send to plane=" + std::to_string(planeId)
                    + " code=" + std::to_string(command.code)
                    + " val(" + std::to_string(command.value[0]) + ","
                              + std::to_string(command.value[1]) + ","
                              + std::to_string(command.value[2]) + ")";
    logTransmission(msg);
    std::cout << msg << std::endl;
}

void CommunicationSystem::logTransmission(const std::string& message) {
    // Ensure log directory exists
    mkdir("/tmp/atc/logs", 0777);
    
    FILE* fp = fopen(transmissionLogPath.c_str(), "a");
    if (fp) {
        fprintf(fp, "%s %s\n", printTimeStamp().c_str(), message.c_str());
        fclose(fp);
    }
}

void CommunicationSystem::run() {
    // open /shm_commands
    int shm_fd = shm_open(SHM_COMMANDS, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "CommunicationSystem: cannot open " << SHM_COMMANDS << "\n";
        return;
    }
    CommandQueue* cq = static_cast<CommandQueue*>(
        mmap(nullptr, sizeof(CommandQueue), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    if (cq == MAP_FAILED) {
        std::cerr << "CommunicationSystem: cannot mmap " << SHM_COMMANDS << "\n";
        close(shm_fd);
        return;
    }

    // loop reading new commands
    while (true) {
        if (cq->head != cq->tail) {
            Command cmd = cq->commands[cq->head];
            cq->head = (cq->head + 1) % MAX_COMMANDS;

            send(cmd.planeId, cmd);
        } else {
            sleep(1);
        }
    }

    munmap(cq, sizeof(CommandQueue));
    close(shm_fd);
}