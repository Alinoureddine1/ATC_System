# Air Traffic Control System User Manual

## Introduction

This user manual describes how to build, run, and use the Air Traffic Control (ATC) system for COEN 320. The system simulates an en-route air traffic control environment with multiple subsystems communicating via shared memory.

## Building the System

### Prerequisites

- QNX Software Development Platform 7.0 or later for x86 Targets
- QNX Momentics IDE
- A QNX RTOS environment (physical machine or virtual machine)

### Build Process

1. Open QNX Momentics IDE
2. Import the project
3. Build the project using the provided Makefile (make sure the QNX IDE uses our custom Makefile): 
   ```
   $ make clean
   $ make all 
   ```

This will create the following executables in the build directory:
- ATCController
- Radar
- ComputerSystem
- OperatorConsole
- DataDisplay
- CommunicationSystem
- AirspaceLogger

## System Setup

1. Create the required directory structure:
   ```
   $ mkdir -p /tmp/atc
   ```

2. Prepare an input file for aircraft data:
   Create a file at `/tmp/atc/plane_input.txt` with the following format for each line:
   ```
   <Time> <ID> <X> <Y> <Z> <SpeedX> <SpeedY> <SpeedZ>
   ```
   Example:
   ```
   0 1 10000 20000 15000 100 50 0
   5 2 30000 40000 18000 -75 25 0
   10 3 80000 10000 20000 -50 100 0
   ```

## Running the System

1. Navigate to the build directory where the executables are located
2. Run the main controller:
   ```
   $ ./ATCController
   ```

The ATCController will start all other subsystems automatically.

## Operator Console Commands

The OperatorConsole provides the following commands:

1. **Display Plane Information**:
   ```
   show_plane <planeId>
   ```
   Example: `show_plane 2`

2. **Set Aircraft Velocity**:
   ```
   set_velocity <planeId> <vx> <vy> <vz>
   ```
   Example: `set_velocity 1 100 50 0`

3. **Update Congestion Parameter**:
   ```
   update_congestion <seconds>
   ```
   Example: `update_congestion 120`

## System Monitoring

### Log Files

The system creates several log files in the `/tmp/atc/logs/` directory:

- `system.log`: Main system logs
- `radar.log`: Radar subsystem logs
- `computer_system.log`: Computer system logs
- `operator_console.log`: Operator console logs
- `data_display.log`: Data display logs
- `communication_system.log`: Communication system logs
- `airspacelog.txt`: Airspace state history
- `commandlog.txt`: Controller command history
- `transmissionlog.txt`: Communication transmission logs
- `plane.log`: Individual aircraft logs

### Alerts

The system will display alerts in the following cases:

1. **Immediate Separation Violation**: When aircraft are currently violating minimum separation distances.
2. **Predicted Separation Violation**: When aircraft are predicted to violate separation within the congestion parameter timeframe.
3. **System-Wide Alerts**: For critical system conditions.

## Shutdown

To shut down the system, press `Ctrl+C` in the ATCController terminal window. The controller will terminate all subsystems and clean up shared memory resources.

## Troubleshooting

### Common Issues

1. **Shared Memory Access Errors**:
   - Ensure no previous instances of the ATC system are running
   - Check log files for specific errors
   - Run `ipcs -m` to check if shared memory segments exist and remove with `ipcrm -m <id>` if needed

2. **Process Connection Failures**:
   - Check if all executables are in the same directory
   - Verify permissions on executables
   - Check log files for channel creation errors

3. **Missing Log Files**:
   - Ensure the `/tmp/atc/logs` directory exists and is writable 
   - Check for disk space issues

### Log Level Adjustment

You can adjust the log level in the code by modifying the call to `setLogLevel()` in the main controller. The available log levels are:
- `LOG_DEBUG`: Most verbose, includes all debug information
- `LOG_INFO`: General operation information
- `LOG_WARNING`: Warnings and errors
- `LOG_ERROR`: Only critical errors