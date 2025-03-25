#include "OperatorConsole.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <queue>
#include <atomic>
#include <sstream>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include "utils.h"

pthread_mutex_t OperatorConsole::mutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<OperatorConsoleResponseMessage> OperatorConsole::responseQueue;

OperatorConsole::OperatorConsole(const std::string& path)
    : commandLogPath(path), chid(-1)
{
}

int OperatorConsole::getChid() const {
    return chid;
}

void OperatorConsole::registerChannelId() {
    int shmChannelsFd = shm_open(SHM_CHANNELS, O_RDWR, 0666);
    if (shmChannelsFd == -1) {
        std::cerr << "OperatorConsole: Cannot open channel IDs shared memory\n";
        return;
    }
    
    ChannelIds* channels = (ChannelIds*)mmap(nullptr, sizeof(ChannelIds), 
                                            PROT_READ|PROT_WRITE, MAP_SHARED, shmChannelsFd, 0);
    if (channels == MAP_FAILED) {
        std::cerr << "OperatorConsole: Cannot map channel IDs shared memory\n";
        close(shmChannelsFd);
        return;
    }
    
    // Register our channel ID
    channels->operatorChid = chid;
    
    munmap(channels, sizeof(ChannelIds));
    close(shmChannelsFd);
    
    std::cout << "OperatorConsole: Registered channel ID " << chid << " in shared memory\n";
}

void OperatorConsole::run() {
    // Ensure log directory exists
    mkdir("/tmp/atc/logs", 0777);
    
    chid = ChannelCreate(0);
    if (chid == -1) {
        std::cout << "OperatorConsole: ChannelCreate fail.\n";
        return;
    }
    
    std::cout << "OperatorConsole: Channel created with ID: " << chid << std::endl;
    
    registerChannelId();

    pthread_t thr;
    std::atomic_bool stop(false);
    pthread_create(&thr, nullptr, &OperatorConsole::cinRead, &stop);

    displayMenu();

    listen();

    stop = true;
    pthread_join(thr, nullptr);
    ChannelDestroy(chid);
}

void OperatorConsole::listen() {
    OperatorConsoleCommandMessage msg;
    int rcvid;

    while (true) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1) {
            std::cerr << "OperatorConsole: MsgReceive fail.\n";
            continue;
        }
        switch(msg.systemCommandType) {
            case OPCON_CONSOLE_COMMAND_GET_USER_COMMAND: {
                pthread_mutex_lock(&mutex);
                if (responseQueue.empty()) {
                    OperatorConsoleResponseMessage resp;
                    resp.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
                    MsgReply(rcvid, EOK, &resp, sizeof(resp));
                } else {
                    OperatorConsoleResponseMessage resp = responseQueue.front();
                    responseQueue.pop();
                    MsgReply(rcvid, EOK, &resp, sizeof(resp));
                }
                pthread_mutex_unlock(&mutex);
                break;
            }
            case OPCON_CONSOLE_COMMAND_ALERT: {
                std::cout << printTimeStamp() << " [OperatorConsole] ALERT: planes "
                          << msg.plane1 << " & " << msg.plane2
                          << " collision in " << msg.collisionTimeSeconds << "s\n";
                OperatorConsoleResponseMessage r;
                r.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
                MsgReply(rcvid, EOK, &r, sizeof(r));
                break;
            }
            case COMMAND_EXIT_THREAD:
                MsgReply(rcvid, EOK, nullptr, 0);
                return;
            default:
                std::cout << "OperatorConsole: unknown sysCommand=" << msg.systemCommandType << "\n";
                MsgError(rcvid, ENOSYS);
                break;
        }
    }
}

void* OperatorConsole::cinRead(void* param) {
    std::atomic_bool* stop = static_cast<std::atomic_bool*>(param);

    int fd = open("/data/home/qnxuser/commandlog.txt", O_CREAT|O_WRONLY|O_APPEND, 0666);
    if (fd == -1) {
        std::cout << "OperatorConsole: cannot open commandlog.\n";
    }

    while (!(*stop)) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            sleep(1);
            continue;
        }
        if (line.empty()) continue;

        if (fd != -1) {
            std::string t = printTimeStamp() + " " + line + "\n";
            write(fd, t.c_str(), t.size());
        }

        std::vector<std::string> tokens;
        tokenize(tokens, line);
        if (tokens.empty()) continue;

        if (tokens[0] == OPCON_COMMAND_STRING_SHOW_PLANE) {
            if (tokens.size()<2) {
                std::cout << "Usage: show_plane <planeId>\n";
                continue;
            }
            int planeId = std::stoi(tokens[1]);
            OperatorConsoleResponseMessage r;
            r.userCommandType = OPCON_USER_COMMAND_DISPLAY_PLANE_INFO;
            r.planeNumber = planeId;
            pthread_mutex_lock(&mutex);
            responseQueue.push(r);
            pthread_mutex_unlock(&mutex);

        } else if (tokens[0] == OPCON_COMMAND_STRING_SET_VELOCITY) {
            if (tokens.size()<5) {
                std::cout << "Usage: set_velocity <planeId> <vx> <vy> <vz>\n";
                continue;
            }
            int planeId = std::stoi(tokens[1]);
            double vx = std::stod(tokens[2]);
            double vy = std::stod(tokens[3]);
            double vz = std::stod(tokens[4]);
            OperatorConsoleResponseMessage r;
            r.userCommandType = OPCON_USER_COMMAND_SET_PLANE_VELOCITY;
            r.planeNumber = planeId;
            r.newVelocity = {vx, vy, vz};
            pthread_mutex_lock(&mutex);
            responseQueue.push(r);
            pthread_mutex_unlock(&mutex);

        } else if (tokens[0] == OPCON_COMMAND_STRING_UPDATE_CONGESTION) {
            if (tokens.size()<2) {
                std::cout << "Usage: update_congestion <seconds>\n";
                continue;
            }
            int c = std::stoi(tokens[1]);
            OperatorConsoleResponseMessage r;
            r.userCommandType = OPCON_USER_COMMAND_UPDATE_CONGESTION;
            r.newCongestionValue = c;
            pthread_mutex_lock(&mutex);
            responseQueue.push(r);
            pthread_mutex_unlock(&mutex);

        } else {
            std::cout << "Unknown console cmd: " << tokens[0] << "\n";
        }
    }

    if (fd != -1) close(fd);
    return nullptr;
}

void OperatorConsole::tokenize(std::vector<std::string>& dest, std::string& str) {
    std::stringstream ss(str);
    std::string tok;
    while (std::getline(ss, tok, ' ')) {
        dest.push_back(tok);
    }
}

void OperatorConsole::logCommand(const std::string& cmd) {
    (void)cmd;
}

void OperatorConsole::displayMenu() {
    std::cout<<"OperatorConsole Commands:\n"
             <<"  show_plane <planeId>\n"
             <<"  set_velocity <planeId> <vx> <vy> <vz>\n"
             <<"  update_congestion <sec>\n";
}

bool OperatorConsole::processInput() {
    return false;
}

void OperatorConsole::sendCommand(const Command& command) {
    std::cout << "[OperatorConsole] direct send plane=" << command.planeId
              << " code="<<command.code<<"\n";
}

void OperatorConsole::requestPlaneInfo(int planeId) {
    OperatorConsoleResponseMessage r;
    r.userCommandType = OPCON_USER_COMMAND_DISPLAY_PLANE_INFO;
    r.planeNumber = planeId;
    pthread_mutex_lock(&mutex);
    responseQueue.push(r);
    pthread_mutex_unlock(&mutex);
}
