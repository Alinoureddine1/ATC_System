# Air Traffic Control (ATC) System

A real-time Air Traffic Control simulation system built for QNX, featuring separate subsystems that communicate using POSIX shared memory and QNX message passing.

## Overview

This project implements a simplified en-route Air Traffic Control system. The system monitors aircraft for separation violations, processes operator commands, and maintains a visual and logged representation of the airspace.

## Additional Documentation

Supplementary documentation is available in the docs directory:
- [System Architecture](docs/system-architecture.md) - Detailed component interactions and design
- [User Manual](docs/user-manual.md) - Comprehensive guide for using the system
- [Test Scenarios](docs/test-scenarios.md) - Test cases for system verification

## Key Features

- Real-time tracking of multiple aircraft
- Inter-process communication via QNX shared memory and message passing
- Collision prediction and alerts
- Interactive operator console for commands
- Visual airspace display with grid representation
- Comprehensive logging system

## System Architecture

The system consists of the following components, each implemented as a separate QNX executable:

- **ATCController**: Master process that initializes shared memory, spawns all subsystems, and monitors their health
- **Radar**: Updates aircraft positions and velocities every second and writes to `/shm_radar_data`
- **ComputerSystem**: Core processing unit that detects separation violations, routes commands, and coordinates displays
- **DataDisplay**: Visualizes the airspace as a grid and shows detailed aircraft information
- **OperatorConsole**: Accepts user commands for querying plane data, changing velocities, etc.
- **CommunicationSystem**: Simulates command transmission to aircraft
- **AirspaceLogger**: Records airspace history to permanent storage

## Repository Structure

The repository is organized as follows:

```
ATC/
├── src/
│   ├── include/
│   │   ├── AirspaceLogger.h
│   │   ├── commandCodes.h
│   │   ├── CommunicationSystem.h
│   │   ├── ComputerSystem.h
│   │   ├── DataDisplay.h
│   │   ├── OperatorConsole.h
│   │   ├── Plane.h
│   │   ├── Radar.h
│   │   ├── shm_utils.h 
|   |   └── utils.h
│   ├── subsystems/
│   │   ├── AirspaceLogger.cpp
│   │   ├── CommunicationSystem.cpp
│   │   ├── ComputerSystem.cpp
│   │   ├── DataDisplay.cpp
│   │   ├── OperatorConsole.cpp
│   │   ├── Plane.cpp
│   │   └── Radar.cpp
│   └── main/
│       ├── ATCController.cpp
│       ├── RadarMain.cpp
│       ├── ComputerSystemMain.cpp
│       ├── OperatorConsoleMain.cpp
│       ├── DataDisplayMain.cpp
│       ├── CommunicationSystemMain.cpp
│       └── AirspaceLoggerMain.cpp
├── build/   # Build output (executables, object files)
├── Makefile
└── README.md   # This file
```

## IPC Implementation Details

### Shared Memory

- `/shm_radar_data`: Stores current aircraft positions and velocities
- `/shm_commands`: Command queue for transmission to aircraft
- `/shm_channels`: Channel IDs for QNX message passing
- `/shm_sync_ready`: Synchronization flag for system startup

### QNX Message Passing

- Uses fixed channels with registered Process IDs for reliable communication
- Employs embedded arrays in message structures
- Implements immediate message replies to prevent blocking

## Prerequisites

- QNX Software Development Platform 7.0 or newer
- QNX Momentics IDE

## Build Instructions

From the project root:

```bash
# Clean previous build
make clean

# Build all executables
make
```

This will create executables in `build/x86_64-debug/`.

### Deployment on QNX Target

```bash
# Create target directory
mkdir -p /tmp/atc
mkdir -p /tmp/atc/logs

# Copy executables
cp build/x86_64-debug/* /tmp/atc/
```

## Running the System

Start ATCController (main process):

```bash
cd /tmp/atc
./ATCController
```

### Startup Verification

- Observe logs confirming each subsystem has started
- Verify that all channel IDs are properly registered
- Check that the grid display starts updating periodically

## Operator Console Commands

- `show_plane <planeId>`: Displays detailed information about the specified plane
- `set_velocity <planeId> <vx> <vy> <vz>`: Updates the velocity of the specified plane
- `update_congestion <seconds>`: Adjusts the look-ahead time for collision prediction

## Log Files

Log files are created in `/tmp/atc/logs/`:

- `system.log`: General system messages
- `radar.log`: Aircraft tracking information
- `computer_system.log`: Processing and violation detection logs
- `operator_console.log`: User command history
- `data_display.log`: Display rendering logs
- `communication_system.log`: Command transmission logs
- `airspace_logger.log`: Periodic airspace snapshots
- `commandlog.txt`: Record of all operator commands
- `airspacelog.txt`: Full history of the airspace state
- `transmissionlog.txt`: Record of all transmissions to aircraft

## Troubleshooting

### Channel ID Issues

- Ensure all processes are running properly
- Check that no previous instances of the system are running
- Verify the shared memory segments are properly created

### Segmentation Faults

- Verify that no message structures contain pointers
- Ensure all message passing uses the proper process ID in ConnectAttach calls
- Check that messages are properly validated before use

### Grid Not Updating

- Verify that the Radar system is running
- Check the radar data in shared memory
- Ensure the ComputerSystem is sending grid updates


## Plane Input File (Optional)

According to the assignment, aircraft information should be read from an input file. To enable this:

1. Create a file on the QNX target (e.g., `/tmp/plane_input.txt`) with the following format:

   ```
   Time ID X Y Z SpeedX SpeedY SpeedZ
   0 1 10000 20000 5000 100 50 0
   0 2 30000 40000 7000 -50 100 0
   10 3 5000 5000 6000 30 0 0
   ```

## Acknowledgments

- Project design based on COEN 320 course specifications
- Implementation uses QNX 7.0 real-time operating system features

## References

- **Project Specification:** Refer to `docs/coen320_project_description.pdf` for complete requirements.
- **QNX Documentation:** QNX Developer's Guide