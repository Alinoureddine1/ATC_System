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
#include "shm_utils.h"

pthread_mutex_t OperatorConsole::mutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<OperatorConsoleResponseMessage> OperatorConsole::responseQueue;

OperatorConsole::OperatorConsole(const std::string &path)
    : commandLogPath(path), chid(-1)
{
    logOperatorConsoleMessage("OperatorConsole initialized with command log: " + path);
}

int OperatorConsole::getChid() const
{
    return chid;
}

void OperatorConsole::registerChannelId()
{
    bool success = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_RDWR,
        false,
        [this](ChannelIds *channels)
        {
            // Register channel ID
            channels->operatorChid = OPERATOR_CONSOLE_CHANNEL_ID;
            logOperatorConsoleMessage("Registered channel ID " + std::to_string(chid) + " in shared memory");
        });

    if (!success)
    {
        logOperatorConsoleMessage("Failed to register channel ID in shared memory", LOG_ERROR);
    }
}

void OperatorConsole::run()
{
    // Ensure log directory exists
    ensureLogDirectories();

    chid = ChannelCreate(OPERATOR_CONSOLE_CHANNEL_ID);
    
    if (chid == -1)
    {
        logOperatorConsoleMessage("ChannelCreate failed: " + std::string(strerror(errno)), LOG_ERROR);
        return;
    }

    logOperatorConsoleMessage("Channel created with ID: " + std::to_string(chid));

    registerChannelId();

    // Start thread to read from stdin
    pthread_t thr;
    std::atomic_bool stop(false);
    pthread_create(&thr, nullptr, &OperatorConsole::cinRead, &stop);

    // Display the menu options
    displayMenu();

    // Listen for messages
    listen();

    // Clean up
    stop = true;
    pthread_join(thr, nullptr);
    ChannelDestroy(chid);

    logOperatorConsoleMessage("OperatorConsole shutdown complete");
}

void OperatorConsole::listen()
{
    OperatorConsoleCommandMessage msg;
    int rcvid;

    logOperatorConsoleMessage("Starting message processing loop");

    while (true)
    {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), nullptr);
        if (rcvid == -1)
        {
            logOperatorConsoleMessage("MsgReceive failed: " + std::string(strerror(errno)), LOG_ERROR);
            continue;
        }

        switch (msg.systemCommandType)
        {
        case OPCON_CONSOLE_COMMAND_GET_USER_COMMAND:
        {
            pthread_mutex_lock(&mutex);

            OperatorConsoleResponseMessage resp;
            if (responseQueue.empty())
            {
                resp.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
                MsgReply(rcvid, EOK, &resp, sizeof(resp));
                logOperatorConsoleMessage("No user commands available", LOG_DEBUG);
            }
            else
            {
                resp = responseQueue.front();
                responseQueue.pop();
                MsgReply(rcvid, EOK, &resp, sizeof(resp));

                std::string cmdTypeStr;
                switch (resp.userCommandType)
                {
                case OPCON_USER_COMMAND_DISPLAY_PLANE_INFO:
                    cmdTypeStr = "Display plane info for plane " + std::to_string(resp.planeNumber);
                    break;
                case OPCON_USER_COMMAND_UPDATE_CONGESTION:
                    cmdTypeStr = "Update congestion value to " + std::to_string(resp.newCongestionValue);
                    break;
                case OPCON_USER_COMMAND_SET_PLANE_VELOCITY:
                    cmdTypeStr = "Set velocity for plane " + std::to_string(resp.planeNumber) +
                                 " to (" + std::to_string(resp.newVelocity.x) + "," +
                                 std::to_string(resp.newVelocity.y) + "," +
                                 std::to_string(resp.newVelocity.z) + ")";
                    break;
                default:
                    cmdTypeStr = "Unknown command type " + std::to_string(resp.userCommandType);
                    break;
                }

                logOperatorConsoleMessage("Sent command to Computer System: " + cmdTypeStr);
            }

            pthread_mutex_unlock(&mutex);
            break;
        }

        case OPCON_CONSOLE_COMMAND_ALERT:
        {
            std::string alertMsg;

            if (msg.plane1 == -1 && msg.plane2 == -1)
            {
                alertMsg = "SYSTEM-WIDE ALERT: Emergency situation detected!";
            }
            else
            {
                alertMsg = "ALERT: Planes " + std::to_string(msg.plane1) +
                           " & " + std::to_string(msg.plane2) +
                           " possible collision in " +
                           std::to_string(msg.collisionTimeSeconds) + "s";
            }

            logOperatorConsoleMessage(alertMsg, LOG_WARNING);

            OperatorConsoleResponseMessage r;
            r.userCommandType = OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE;
            MsgReply(rcvid, EOK, &r, sizeof(r));
            break;
        }

        case COMMAND_EXIT_THREAD:
            logOperatorConsoleMessage("Received exit command");
            MsgReply(rcvid, EOK, nullptr, 0);
            return;

        default:
            logOperatorConsoleMessage("Unknown system command: " +
                                          std::to_string(msg.systemCommandType),
                                      LOG_WARNING);
            MsgError(rcvid, ENOSYS);
            break;
        }
    }
}

void *OperatorConsole::cinRead(void *param)
{
    std::atomic_bool *stop = static_cast<std::atomic_bool *>(param);

    // Open command log file
    FILE *fd = fopen("/tmp/atc/logs/commandlog.txt", "a");
    if (fd == nullptr)
    {
        logOperatorConsoleMessage("Failed to open command log: " +
                                      std::string(strerror(errno)),
                                  LOG_ERROR);
    }
    else
    {
        logOperatorConsoleMessage("Command log opened successfully");
    }

    // Main input processing loop
    while (!(*stop))
    {
        std::string line;
        if (!std::getline(std::cin, line))
        {
            sleep(1);
            continue;
        }

        if (line.empty())
        {
            continue;
        }

        // Log the command
        if (fd != nullptr)
        {
            std::string t = printTimeStamp() + " " + line + "\n";
            if (fwrite(t.c_str(), 1, t.size(), fd) != t.size())
            {
                logOperatorConsoleMessage("Failed to write to command log", LOG_WARNING);
            }
            fflush(fd);
        }

        // Parse the input into tokens
        std::vector<std::string> tokens;
        tokenize(tokens, line);
        if (tokens.empty())
            continue;

        // Process based on command type
        try
        {
            if (tokens[0] == OPCON_COMMAND_STRING_SHOW_PLANE)
            {
                if (tokens.size() < 2)
                {
                    logOperatorConsoleMessage("Usage: show_plane <planeId>", LOG_WARNING);
                    continue;
                }

                int planeId = std::stoi(tokens[1]);
                logOperatorConsoleMessage("Processing show_plane command for plane " +
                                          std::to_string(planeId));

                OperatorConsoleResponseMessage r;
                r.userCommandType = OPCON_USER_COMMAND_DISPLAY_PLANE_INFO;
                r.planeNumber = planeId;

                pthread_mutex_lock(&mutex);
                responseQueue.push(r);
                pthread_mutex_unlock(&mutex);
            }
            else if (tokens[0] == OPCON_COMMAND_STRING_SET_VELOCITY)
            {
                if (tokens.size() < 5)
                {
                    logOperatorConsoleMessage("Usage: set_velocity <planeId> <vx> <vy> <vz>", LOG_WARNING);
                    continue;
                }

                int planeId = std::stoi(tokens[1]);
                double vx = std::stod(tokens[2]);
                double vy = std::stod(tokens[3]);
                double vz = std::stod(tokens[4]);

                logOperatorConsoleMessage("Processing set_velocity command for plane " +
                                          std::to_string(planeId) +
                                          " to (" + std::to_string(vx) + "," +
                                          std::to_string(vy) + "," +
                                          std::to_string(vz) + ")");

                OperatorConsoleResponseMessage r;
                r.userCommandType = OPCON_USER_COMMAND_SET_PLANE_VELOCITY;
                r.planeNumber = planeId;
                r.newVelocity = {vx, vy, vz};

                pthread_mutex_lock(&mutex);
                responseQueue.push(r);
                pthread_mutex_unlock(&mutex);
            }
            else if (tokens[0] == OPCON_COMMAND_STRING_UPDATE_CONGESTION)
            {
                if (tokens.size() < 2)
                {
                    logOperatorConsoleMessage("Usage: update_congestion <seconds>", LOG_WARNING);
                    continue;
                }

                int c = std::stoi(tokens[1]);
                logOperatorConsoleMessage("Processing update_congestion command with value " +
                                          std::to_string(c));

                OperatorConsoleResponseMessage r;
                r.userCommandType = OPCON_USER_COMMAND_UPDATE_CONGESTION;
                r.newCongestionValue = c;

                pthread_mutex_lock(&mutex);
                responseQueue.push(r);
                pthread_mutex_unlock(&mutex);
            }
            else
            {
                logOperatorConsoleMessage("Unknown command: " + tokens[0], LOG_WARNING);
            }
        }
        catch (const std::exception &e)
        {
            logOperatorConsoleMessage("Error processing command: " +
                                          std::string(e.what()),
                                      LOG_ERROR);
        }
    }

    // Close log file
    if (fd != nullptr)
    {
        fclose(fd);
    }

    return nullptr;
}

void OperatorConsole::tokenize(std::vector<std::string> &dest, std::string &str)
{
    std::stringstream ss(str);
    std::string tok;
    while (std::getline(ss, tok, ' '))
    {
        if (!tok.empty())
        {
            dest.push_back(tok);
        }
    }
}

void OperatorConsole::logCommand(const std::string &cmd)
{
    logOperatorConsoleMessage("User command: " + cmd);
}

void OperatorConsole::displayMenu()
{
    std::string menu =
        "\nOperatorConsole Commands:\n"
        "  show_plane <planeId>              - Show detailed information about a plane\n"
        "  set_velocity <planeId> <vx> <vy> <vz> - Set velocity for a plane\n"
        "  update_congestion <sec>           - Update congestion parameter in seconds\n";

    std::cout << menu << std::endl;
    logOperatorConsoleMessage("Displayed command menu");
}

bool OperatorConsole::processInput()
{
    // Not used in this implementation
    return false;
}

void OperatorConsole::sendCommand(const Command &command)
{
    logOperatorConsoleMessage("Direct send to plane " + std::to_string(command.planeId) +
                              " with code " + std::to_string(command.code));
}

void OperatorConsole::requestPlaneInfo(int planeId)
{
    logOperatorConsoleMessage("Requesting info for plane " + std::to_string(planeId));

    OperatorConsoleResponseMessage r;
    r.userCommandType = OPCON_USER_COMMAND_DISPLAY_PLANE_INFO;
    r.planeNumber = planeId;

    pthread_mutex_lock(&mutex);
    responseQueue.push(r);
    pthread_mutex_unlock(&mutex);
}

void *OperatorConsole::start(void *context)
{
    auto op = static_cast<OperatorConsole *>(context);
    op->run();
    return nullptr;
}