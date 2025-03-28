#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sys/wait.h>
#include "commandCodes.h"
#include "utils.h"
#include "shm_utils.h"

static volatile sig_atomic_t running = 1;
static pid_t childPids[6] = {-1, -1, -1, -1, -1, -1};

static void handleSig(int sig) {
    running = 0;
    
    for (int i = 0; i < 6; i++) {
        if (childPids[i] > 0) {
            kill(childPids[i], SIGTERM);
        }
    }
}

void monitorSystemStatus() {
    logSystemMessage("System monitoring thread started");
    
    while (running) {
        bool syncSuccess = accessSharedMemory<int>(
            SHM_SYNC_READY,
            sizeof(int),
            O_RDONLY,
            false,
            [](int* syncReady) {
                if (*syncReady == 0) {
                    logSystemMessage("WARNING: Shared memory sync flag reset to 0!", LOG_WARNING);
                    
                    bool restoreSuccess = accessSharedMemory<int>(
                        SHM_SYNC_READY,
                        sizeof(int),
                        O_RDWR,
                        false,
                        [](int* writeSync) {
                            *writeSync = 1;
                        }
                    );
                    
                    if (!restoreSuccess) {
                        logSystemMessage("Failed to restore sync flag", LOG_ERROR);
                    }
                }
            }
        );
        
        if (!syncSuccess) {
            logSystemMessage("Failed to check sync flag", LOG_WARNING);
        }
        
        // Check channel IDs to ensure all subsystems are still registered
        bool channelsSuccess = accessSharedMemory<ChannelIds>(
            SHM_CHANNELS,
            sizeof(ChannelIds),
            O_RDONLY,
            false,
            [](ChannelIds* channels) {
                if (channels->operatorChid <= 0 || channels->operatorPid <= 0 || 
                    channels->displayChid <= 0 || channels->displayPid <= 0 || 
                    channels->loggerChid <= 0 || channels->loggerPid <= 0 || 
                    channels->computerChid <= 0 || channels->computerPid <= 0) {
                    
                    logSystemMessage("WARNING: One or more subsystem channel IDs or PIDs are missing or invalid!", LOG_WARNING);
                    
                    if (channels->operatorChid <= 0 || channels->operatorPid <= 0) 
                        logSystemMessage("OperatorConsole channel ID or PID missing", LOG_WARNING);
                    if (channels->displayChid <= 0 || channels->displayPid <= 0) 
                        logSystemMessage("DataDisplay channel ID or PID missing", LOG_WARNING);
                    if (channels->loggerChid <= 0 || channels->loggerPid <= 0) 
                        logSystemMessage("AirspaceLogger channel ID or PID missing", LOG_WARNING);
                    if (channels->computerChid <= 0 || channels->computerPid <= 0) 
                        logSystemMessage("ComputerSystem channel ID or PID missing", LOG_WARNING);
                }
            }
        );
        
        if (!channelsSuccess) {
            logSystemMessage("Failed to check channel IDs", LOG_WARNING);
        }
        
        sleep(5); 
    }
    
    logSystemMessage("System monitoring thread terminated");
}

bool initializeSystemComponents() {
    mkdir("/tmp/atc", 0777);
    mkdir("/tmp/atc/logs", 0777);

    bool syncSuccess = accessSharedMemory<int>(
        SHM_SYNC_READY,
        sizeof(int),
        O_CREAT | O_RDWR,
        true,
        [](int* sync) {
            *sync = 0; 
        }
    );
    
    if (!syncSuccess) {
        logSystemMessage("Failed to create sync flag shared memory", LOG_ERROR);
        return false;
    }

    // Create radar data shared memory
    bool radarSuccess = accessSharedMemory<RadarData>(
        SHM_RADAR_DATA,
        sizeof(RadarData),
        O_CREAT | O_RDWR,
        true,
        [](RadarData* rd) {
            rd->numPlanes = 0;
            for (int i = 0; i < MAX_PLANES; i++) {
                rd->positions[i].planeId = -1;
                rd->positions[i].x = 0;
                rd->positions[i].y = 0;
                rd->positions[i].z = 0;
                rd->positions[i].timestamp = 0;
                
                rd->velocities[i].planeId = -1;
                rd->velocities[i].vx = 0;
                rd->velocities[i].vy = 0;
                rd->velocities[i].vz = 0;
                rd->velocities[i].timestamp = 0;
            }
        }
    );
    
    if (!radarSuccess) {
        logSystemMessage("Failed to create radar data shared memory", LOG_ERROR);
        return false;
    }

    bool cmdSuccess = accessSharedMemory<CommandQueue>(
        SHM_COMMANDS,
        sizeof(CommandQueue),
        O_CREAT | O_RDWR,
        true,
        [](CommandQueue* cq) {
            cq->head = 0;
            cq->tail = 0;
            for (int i = 0; i < MAX_COMMANDS; i++) {
                cq->commands[i].planeId = -1;
                cq->commands[i].code = CMD_POSITION;
                cq->commands[i].value[0] = 0;
                cq->commands[i].value[1] = 0;
                cq->commands[i].value[2] = 0;
                cq->commands[i].timestamp = 0;
            }
        }
    );
    
    if (!cmdSuccess) {
        logSystemMessage("Failed to create commands shared memory", LOG_ERROR);
        return false;
    }

    // Create channels shared memory for inter-process communication
    bool channelsSuccess = accessSharedMemory<ChannelIds>(
        SHM_CHANNELS,
        sizeof(ChannelIds),
        O_CREAT | O_RDWR,
        true,
        [](ChannelIds* channels) {
            // Initialize to invalid values to detect when real IDs are registered
            channels->operatorChid = -1;
            channels->operatorPid = -1;  
            channels->displayChid = -1;
            channels->displayPid = -1;   
            channels->loggerChid = -1;
            channels->loggerPid = -1;    
            channels->computerChid = -1;
            channels->computerPid = -1;  
        }
    );
    
    if (!channelsSuccess) {
        logSystemMessage("Failed to create channels shared memory", LOG_ERROR);
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    setLogLevel(LOG_INFO);
    
    logSystemMessage("Air Traffic Control System starting");

    if (!initializeSystemComponents()) {
        logSystemMessage("Failed to initialize system components", LOG_ERROR);
        return 1;
    }

    logSystemMessage("Starting subsystems");

    std::string basePath = "/tmp/atc";

    pid_t pids[6] = {-1, -1, -1, -1, -1, -1};
    
    // Start OperatorConsole first (ID 1)
    pids[4] = fork();
    if (pids[4] == 0) {
        execl((basePath+"/OperatorConsole").c_str(), "OperatorConsole", (char*)nullptr);
        logSystemMessage("Failed to exec OperatorConsole: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[4] = pids[4];  
    sleep(2);  
    
    // Start DataDisplay next (ID 2)
    pids[2] = fork();
    if (pids[2] == 0) {
        execl((basePath+"/DataDisplay").c_str(), "DataDisplay", (char*)nullptr);
        logSystemMessage("Failed to exec DataDisplay: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[2] = pids[2];  
    sleep(2);  
    
    // Start AirspaceLogger next (ID 3)
    pids[5] = fork();
    if (pids[5] == 0) {
        execl((basePath+"/AirspaceLogger").c_str(), "AirspaceLogger", (char*)nullptr);
        logSystemMessage("Failed to exec AirspaceLogger: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[5] = pids[5];  
    sleep(2);  
    
    // Start ComputerSystem next (ID 4)
    pids[1] = fork();
    if (pids[1] == 0) {
        execl((basePath+"/ComputerSystem").c_str(), "ComputerSystem", (char*)nullptr);
        logSystemMessage("Failed to exec ComputerSystem: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[1] = pids[1];  
    sleep(2); 
    
    // Start other components that don't need specific IDs
    pids[0] = fork();    
    if (pids[0] == 0) {
        execl((basePath+"/Radar").c_str(), "Radar", (char*)nullptr);
        logSystemMessage("Failed to exec Radar: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[0] = pids[0];  
    sleep(1);
    
    pids[3] = fork();
    if (pids[3] == 0) {
        execl((basePath+"/CommunicationSystem").c_str(), "CommunicationSystem", (char*)nullptr);
        logSystemMessage("Failed to exec CommunicationSystem: " + std::string(strerror(errno)), LOG_ERROR);
        _exit(1);
    }
    childPids[3] = pids[3];  
    
    // Allow time for all processes to initialize
    sleep(5);
    
    // Signal that shared memory is ready for use
    accessSharedMemory<int>(
        SHM_SYNC_READY,
        sizeof(int),
        O_RDWR,
        false,
        [](int* syncReady) {
            *syncReady = 1; // Ready now
        }
    );
    
    logSystemMessage("All processes started and shared memory initialized");
    
    // Start system monitoring thread
    std::thread monitorThread(monitorSystemStatus);
    monitorThread.detach();

    // Use sigaction for signal handling
    struct sigaction sa;
    sa.sa_handler = handleSig;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    logSystemMessage("System running. Press Ctrl+C to terminate.");
    
    // Main loop - monitor children
    while (running) {
        // Check if any child has crashed
        for (int i = 0; i < 6; i++) {
            if (pids[i] > 0) {
                int status;
                pid_t result = waitpid(pids[i], &status, WNOHANG);
                
                if (result == pids[i]) {
                    // Child has terminated
                    if (WIFEXITED(status)) {
                        logSystemMessage("Child process " + std::to_string(pids[i]) + 
                                       " exited with status " + std::to_string(WEXITSTATUS(status)), 
                                       LOG_WARNING);
                    }
                    else if (WIFSIGNALED(status)) {
                        logSystemMessage("Child process " + std::to_string(pids[i]) + 
                                       " killed by signal " + std::to_string(WTERMSIG(status)), 
                                       LOG_ERROR);
                    }
                    
                    // Mark the PID as invalid
                    pids[i] = -1;
                    childPids[i] = -1;
                }
            }
        }
        
        sleep(1);
    }

    logSystemMessage("Received termination signal, shutting down subsystems", LOG_WARNING);
    
    // Send termination signal to all child processes
    for (int i = 0; i < 6; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGTERM);
        }
    }
    
    // Wait for processes to terminate
    sleep(5); 
    
    // Clean up shared memory
    logSystemMessage("Cleaning up shared memory");
    
    shm_unlink(SHM_RADAR_DATA);
    shm_unlink(SHM_COMMANDS);
    shm_unlink(SHM_CHANNELS);
    shm_unlink(SHM_SYNC_READY);

    logSystemMessage("Shutdown complete");
    return 0;
}