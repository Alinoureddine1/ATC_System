#ifndef COMMAND_CODES_H
#define COMMAND_CODES_H

#include <vector>
#include <ctime>
#include <string>

/** POSIX shared memory names **/
#define SHM_RADAR_DATA "/shm_radar_data"
#define SHM_COMMANDS   "/shm_commands"
#define SHM_CHANNELS   "/shm_channels" 
#define SHM_SYNC_READY "/shm_sync_ready"

#define MAX_PLANES    10
#define MAX_COMMANDS  10

// Default log file paths
#define DEFAULT_AIRSPACE_LOG_PATH "/tmp/atc/logs/airspacelog.txt"
#define DEFAULT_COMMAND_LOG_PATH "/tmp/atc/logs/commandlog.txt"
#define DEFAULT_TRANSMISSION_LOG_PATH "/tmp/atc/logs/transmissionlog.txt"
#define DEFAULT_PLANE_INPUT_PATH "/tmp/atc/plane_input.txt"
#define DEFAULT_SYSTEM_LOG_PATH "/tmp/atc/logs/system.log"
#define DEFAULT_RADAR_LOG_PATH "/tmp/atc/logs/radar.log"
#define DEFAULT_COMPUTER_SYSTEM_LOG_PATH "/tmp/atc/logs/computer_system.log"
#define DEFAULT_PLANE_LOG_PATH "/tmp/atc/logs/plane.log"

/** Command codes **/
enum CommandCode {
    CMD_VELOCITY = 1,
    CMD_POSITION = 2
};


// Fixed channel IDs for subsystems
#define OPERATOR_CONSOLE_CHANNEL_ID  1
#define DATA_DISPLAY_CHANNEL_ID      2
#define AIRSPACE_LOGGER_CHANNEL_ID   3
#define COMPUTER_SYSTEM_CHANNEL_ID   4

/** Timer pulse codes **/
#define AIRSPACE_VIOLATION_CONSTRAINT_TIMER 1
#define LOG_AIRSPACE_TO_CONSOLE_TIMER       2
#define OPERATOR_COMMAND_CHECK_TIMER        3
#define LOG_AIRSPACE_TO_FILE_TIMER          4
#define LOG_AIRSPACE_TO_LOGGER_TIMER        5

// DataDisplay command types
#define COMMAND_ONE_PLANE       6
#define COMMAND_GRID            7
#define COMMAND_LOG             8
#define COMMAND_MULTIPLE_PLANE  9

// IPC command types
#define COMMAND_OPERATOR_REQUEST 10
#define COMMAND_EXIT_THREAD      11
#define COMMAND_LOG_AIRSPACE     100

// OperatorConsole commands
#define OPCON_COMMAND_STRING_SHOW_PLANE       "show_plane"
#define OPCON_COMMAND_STRING_SET_VELOCITY     "set_velocity"
#define OPCON_COMMAND_STRING_UPDATE_CONGESTION "update_congestion"

// OperatorConsole system commands
enum OperatorConsoleSystemCommand {
    OPCON_CONSOLE_COMMAND_GET_USER_COMMAND = 1,
    OPCON_CONSOLE_COMMAND_ALERT = 2
};

enum OperatorConsoleUserCommand {
    OPCON_USER_COMMAND_NO_COMMAND_AVAILABLE = 0,
    OPCON_USER_COMMAND_DISPLAY_PLANE_INFO   = 1,
    OPCON_USER_COMMAND_UPDATE_CONGESTION    = 2,
    OPCON_USER_COMMAND_SET_PLANE_VELOCITY   = 3
};

// Structure to share channel IDs between processes
struct ChannelIds {
    int operatorChid;
    int displayChid;
    int loggerChid;
    int computerChid;
};

// Data structures
struct Vec3 {
    double x, y, z;

    Vec3 sum(const Vec3& other) const {
        return { x + other.x, y + other.y, z + other.z };
    }
    Vec3 scalarMultiplication(double s) const {
        return { x*s, y*s, z*s };
    }
};

struct Position {
    int planeId;
    double x, y, z;
    time_t timestamp;
};

struct Velocity {
    int planeId;
    double vx, vy, vz;
    time_t timestamp;
};

struct Command {
    int planeId;
    CommandCode code;
    double value[3];
    time_t timestamp;
};

struct RadarData {
    int numPlanes;
    Position positions[MAX_PLANES];
    Velocity velocities[MAX_PLANES];
};

struct CommandQueue {
    int head;
    int tail;
    Command commands[MAX_COMMANDS];
};

struct PlanePositionResponse {
    Vec3 currentPosition;
    Vec3 currentVelocity;
};

struct AirspaceLogMessage {
    int commandType;
    std::vector<Position> positions;
    std::vector<Velocity> velocities;
    double timestamp;
};

struct multipleAircraftDisplay {
    size_t numberOfAircrafts;
    int* planeIDArray;
    Vec3* positionArray;
    Vec3* velocityArray;
};

struct dataDisplayCommandMessage {
    int commandType;
    union {
        struct {
            int aircraftID;
            Vec3 position;
            Vec3 velocity;
        } one;
        multipleAircraftDisplay multiple;
    } commandBody;
};

struct OperatorConsoleCommandMessage {
    int systemCommandType;
    int plane1, plane2;
    double collisionTimeSeconds;
};

struct OperatorConsoleResponseMessage {
    int userCommandType;
    int planeNumber;
    double newCongestionValue;
    Vec3 newVelocity;
};

#endif // COMMAND_CODES_H