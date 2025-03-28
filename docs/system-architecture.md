# Air Traffic Control System Architecture

## Overview

This document describes the architecture of the Air Traffic Control (ATC) system implemented for the COEN 320 project. The system simulates an en-route air traffic control system responsible for maintaining adequate aircraft separation distances in a 3D airspace.

## System Components

The ATC system consists of the following major components, each implemented as a separate QNX executable:

1. **ATCController**: The main controller that initializes the system, creates shared memory, and launches all subsystems.
2. **Radar**: Detects aircraft positions and velocities, updating shared memory.
3. **ComputerSystem**: Performs safety checks, violation detection, and processes operator commands.
4. **OperatorConsole**: Provides a command-line interface for the controller to input commands.
5. **DataDisplay**: Visualizes airspace and aircraft information in a grid-based display.
6. **CommunicationSystem**: Simulates the transmission of commands to aircraft.
7. **AirspaceLogger**: Records airspace state history for later analysis.

## Communication Architecture

The subsystems communicate using POSIX shared memory and QNX message passing:

### Shared Memory Segments

- **`/shm_radar_data`**: Contains current aircraft positions and velocities.
- **`/shm_commands`**: Queue of commands to be sent to aircraft.
- **`/shm_channels`**: IDs of communication channels for message passing.
- **`/shm_sync_ready`**: Synchronization flag for system startup.

### Message Passing

Each subsystem creates a message channel for receiving commands:
- Messages between ComputerSystem and OperatorConsole
- Messages between ComputerSystem and DataDisplay
- Messages between ComputerSystem and AirspaceLogger

## Timing and Periodic Tasks

The system implements several periodic tasks:

1. **Aircraft Position Updates**: Every 1 second
2. **Separation Constraint Checking**: Every 1 second
3. **Operator Command Checking**: Every 1 second
4. **Airspace Logging to Console**: Every 5 seconds
5. **Airspace Logging to File**: Every 20 seconds

## Data Flow

1. Radar reads aircraft positions and updates `/shm_radar_data`
2. ComputerSystem reads radar data and checks for constraint violations
3. If violations are detected, ComputerSystem alerts OperatorConsole
4. OperatorConsole allows controller to input commands
5. Commands are queued in `/shm_commands`
6. CommunicationSystem reads commands and simulates transmission to aircraft
7. DataDisplay reads radar data to visualize airspace
8. AirspaceLogger periodically records the state of the airspace

## Safety Mechanisms

The system includes several safety mechanisms:

1. **Separation Constraint Checking**: Ensures aircraft maintain minimum vertical and horizontal separation.
2. **Predictive Violation Detection**: Calculates future positions to detect potential violations.
3. **Emergency Alert System**: Notifies controller of imminent violations.
4. **Command Logging**: Records all operator commands for audit purposes.
5. **Redundant Logging**: Multiple logging systems ensure data preservation.

## Fault Tolerance

The system implements basic fault tolerance:

1. **Process Monitoring**: ATCController monitors subsystem processes.
2. **Shared Memory Resilience**: Retry mechanisms for shared memory access.
3. **Error Logging**: Comprehensive error logging for diagnosis.

## Visualization

The DataDisplay component provides two visualization modes:

1. **Grid Display**: Shows a 2D grid representation of the airspace.
2. **Individual Aircraft View**: Detailed information about a specific aircraft.

## Operator Interface

The OperatorConsole provides a command-line interface with the following commands:

- `show_plane <planeId>`: Display detailed information about a specific aircraft.
- `set_velocity <planeId> <vx> <vy> <vz>`: Change the velocity of an aircraft.
- `update_congestion <seconds>`: Update the prediction time for congestion detection.