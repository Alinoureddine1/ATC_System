# ATC Project

This project implements a simplified, real-time Air Traffic Control (ATC) system for QNX. It is composed of several separate executables (subsystems) that communicate using POSIX shared memory and QNX message passing. The system simulates an en-route ATC environment where aircraft are monitored for separation, operator commands are processed, and system events are logged.

## Overview

The system consists of the following components (each built as a separate QNX executable):

- **ATCController**  
  The master process that sets up shared memory and spawns all subsystem processes.

- **Radar**  
  Simulates primary and secondary surveillance radar functionality. It updates aircraft positions and velocities every second and writes these to the shared memory segment `/shm_radar_data`.

- **ComputerSystem**  
  Reads aircraft data from shared memory, performs separation checks (using a configurable "congestion" parameter to predict future collisions), and sends display commands and alerts.

- **DataDisplay**  
  Receives commands (e.g., show grid or display single-plane info) via QNX message passing and outputs the current airspace state to the console and/or log file.

- **OperatorConsole**  
  Provides an interactive console that accepts user commands to query plane data, update velocities, or change the congestion parameter. Commands are logged and forwarded to ComputerSystem.

- **CommunicationSystem**  
  Simulates transmitting commands to aircraft by reading from the shared memory command queue (`/shm_commands`) and logging the transmission events.

- **AirspaceLogger**  
  Logs the full airspace state (positions and velocities of all aircraft) to a history file at periodic intervals.

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
│   │   └── utils.h
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
├── logs/    # (Optional) Local logs folder
├── Makefile
└── README.md   # This file
```

## Build & Deployment

1. **Install QNX SDP 7.0+** on your VM.
2. **Clone the repository:**
   ```bash
   git clone https://github.com/Alinoureddine1/ATC_System.git
   ```
3. **Build the project:** From the project root, run:
   ```bash
   make
   ```
   Executables will be produced in `build/x86_64-debug/`.

4. **Deploy on QNX Target:** Create a folder (for example, `/tmp/atc`) on your QNX target and copy all executables there:
   ```bash
   mkdir -p /tmp/atc
   cp build/x86_64-debug/ATCController /tmp/atc
   cp build/x86_64-debug/Radar /tmp/atc
   cp build/x86_64-debug/ComputerSystem /tmp/atc
   cp build/x86_64-debug/DataDisplay /tmp/atc
   cp build/x86_64-debug/OperatorConsole /tmp/atc
   cp build/x86_64-debug/CommunicationSystem /tmp/atc
   cp build/x86_64-debug/AirspaceLogger /tmp/atc
   ```

5. **Set Base Path:** In `ATCController.cpp`, ensure the base path is set to the folder containing your executables:
   ```cpp
   std::string basePath = "/tmp/atc";
   ```

## Running the System

### Launching ATCController

In a QNX shell:
```bash
cd /tmp/atc
./ATCController
```

This process will spawn all subsystem processes and print startup messages. It will then wait (using `pause()`) for a signal (such as SIGINT).

### Running OperatorConsole Interactively

Since only the OperatorConsole is interactive, it is best run in its own terminal. If ATCController currently spawns OperatorConsole automatically, consider commenting out that fork block in ATCController.cpp. Then, in a separate QNX shell:

```bash
cd /tmp/atc
./OperatorConsole
```

You can then type the following commands directly:
- `show_plane <planeId>`
- `set_velocity <planeId> <vx> <vy> <vz>`
- `update_congestion <seconds>`

Example:
```
show_plane 1
set_velocity 2 80 20 0
update_congestion 120
```

## Log Files

The system writes logs to:
- OperatorConsole: `/data/home/qnxuser/commandlog.txt`
- AirspaceLogger: `/data/home/qnxuser/logs/airspacelog.txt`
- CommunicationSystem: `/data/home/qnxuser/logs/transmissionlog.txt`
- DataDisplay: May also write to `/data/home/qnxuser/airspacelog.txt`

Ensure the log directory exists on your QNX target:
```bash
mkdir -p /data/home/qnxuser/logs
```

Then view logs using:
```bash
cat /data/home/qnxuser/commandlog.txt
tail -f /data/home/qnxuser/logs/airspacelog.txt
```

## Operator Console Commands

The system supports the following interactive commands:

- **show_plane \<planeId\>**  
  Displays detailed information about the specified plane.

- **set_velocity \<planeId\> \<vx\> \<vy\> \<vz\>**  
  Sends a command to update the velocity of the specified plane.

- **update_congestion \<seconds\>**  
  Adjusts the look-ahead time for collision detection (the "congestion" parameter). For example, setting it to 120 means the system predicts potential collisions 120 seconds into the future.

Any command not matching these will result in an "Unknown console cmd" message.

## Plane Input File (Optional)

According to the assignment, aircraft information should be read from an input file. To enable this:

1. Create a file on the QNX target (e.g., `/tmp/plane_input.txt`) with the following format:
   ```
   Time ID X Y Z SpeedX SpeedY SpeedZ
   0 1 10000 20000 5000 100 50 0
   0 2 30000 40000 7000 -50 100 0
   10 3 5000 5000 6000 30 0 0
   ```
2. Modify `RadarMain.cpp` to parse this file and spawn aircraft according to the "Time" field. (A sample implementation is provided in the code comments.)

## Demo Script

1. **Deploy executables:**  
   Ensure all executables are in `/tmp/atc` and the base path in `ATCController.cpp` is set to `/tmp/atc`.

2. **Open a QNX Terminal Session (Shell #1):**
   ```bash
   cd /tmp/atc
   ./ATCController
   ```
   You should see startup messages from each subsystem.

3. **Open a Second Terminal Session (Shell #2):**  
   Run:
   ```bash
   cd /tmp/atc
   ./OperatorConsole
   ```
   Type commands:
   ```
   show_plane 1
   set_velocity 2 80 20 0
   update_congestion 120
   ```

4. **Monitor Logs:**  
   In a separate shell, check:
   ```bash
   cat /data/home/qnxuser/commandlog.txt
   tail -f /data/home/qnxuser/logs/airspacelog.txt
   cat /data/home/qnxuser/logs/transmissionlog.txt
   ```

5. **Shutdown:**  
   Stop the system by pressing Ctrl+C in the terminal running ATCController, or use:
   ```bash
   slay -f ATCController Radar ComputerSystem DataDisplay OperatorConsole CommunicationSystem AirspaceLogger
   ```

## Additional Notes

- **Multiple Terminals:**  
  If you cannot open multiple windows on your QNX VM directly, use SSH (for example, via PuTTY) to open additional sessions.

- **Congestion Parameter:**  
  This parameter sets the look-ahead time for collision detection. A higher value provides earlier warning but may trigger alerts sooner.

- **Log Directories:**  
  Ensure that `/data/home/qnxuser/logs` exists on your QNX target. If not, create it with `mkdir -p /data/home/qnxuser/logs`.

## References

- **Project Specification:** Refer to `coen320_project_description.F.pdf` for complete requirements.
- **QNX Documentation:** QNX Developer's Guide