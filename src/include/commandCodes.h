#ifndef COMMAND_CODES_H
#define COMMAND_CODES_H

// Shared memory names
#define SHM_NAME_POSITION "shm_position"
#define SHM_NAME_VELOCITY "shm_velocity"
#define SHM_NAME_COMMANDS "shm_commands"

// Message types for subsystem communication
enum MessageType {
    MSG_TYPE_POSITION = 1,
    MSG_TYPE_VELOCITY,
    MSG_TYPE_COMMAND,
    MSG_TYPE_ALERT
};

// Command codes for operator console
enum CommandCode {
    CMD_VELOCITY = 1,
    CMD_POSITION,
    CMD_SET_N,
    CMD_SHOW_PLANE
};

// Structs for shared memory data
struct Position {
    int planeId;
    double x, y, z;
    double timestamp;
};

struct Velocity {
    int planeId;
    double vx, vy, vz;
    double timestamp;
};

struct Command {
    int planeId;
    CommandCode code;
    double value[3]; // Used for velocity (vx, vy, vz) or position (x, y, z)
    double timestamp;
};

#endif // COMMAND_CODES_H