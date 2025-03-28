# ATC System Test Scenarios

This document outlines test scenarios to verify the functionality and performance of the Air Traffic Control system.

## 1. Basic Functionality Tests

### Test 1.1: System Startup and Initialization

**Purpose:** Verify that all subsystems start correctly and establish communication.

**Input:** None

**Expected Output:**
- All processes start successfully
- Shared memory segments are created
- Channel IDs are registered
- Log files are created
- System reports "All processes started and shared memory initialized"

**Testing Procedure:**
1. Run the ATCController executable
2. Check the system.log for initialization messages
3. Verify all subsystems are running using `ps -A | grep ATC`

### Test 1.2: Aircraft Detection

**Purpose:** Verify that the Radar subsystem correctly detects aircraft.

**Input:** 
Create file `/tmp/atc/plane_input.txt` with:
```
0 1 10000 20000 15000 100 50 0
```

**Expected Output:**
- Radar logs detection of plane 1
- Plane position is written to /shm_radar_data
- Computer system reads the data

**Testing Procedure:**
1. Start the system
2. Check radar.log for aircraft detection messages
3. Check computer_system.log for aircraft processing messages

### Test 1.3: Data Display Visualization

**Purpose:** Verify that the DataDisplay subsystem correctly visualizes airspace.

**Input:** Same as Test 1.2

**Expected Output:**
- DataDisplay shows a grid with aircraft position
- Aircraft ID 1 appears in the correct grid cell

**Testing Procedure:**
1. Start the system with Test 1.2 input
2. Check data_display.log for grid visualization output

### Test 1.4: Operator Commands

**Purpose:** Verify that the OperatorConsole correctly processes user commands.

**Input:** 
After system startup, enter:
```
show_plane 1
set_velocity 1 200 100 0
```

**Expected Output:**
- First command: Detailed information about plane 1 is displayed
- Second command: System logs velocity update and command is queued

**Testing Procedure:**
1. Start the system with Test 1.2 input
2. Enter commands in the OperatorConsole window
3. Check operator_console.log and communication_system.log for command processing

## 2. Safety Features Tests

### Test 2.1: Collision Detection - Current Violation

**Purpose:** Verify that the system detects current separation violations.

**Input:** 
Create file `/tmp/atc/plane_input.txt` with:
```
0 1 10000 20000 15000 100 50 0
0 2 11000 21000 15500 -100 -50 0
```

**Expected Output:**
- System detects that planes are within minimum separation
- Alert message is sent to OperatorConsole
- Violation is logged in computer_system.log

**Testing Procedure:**
1. Start the system with the input file
2. Check computer_system.log for violation detection
3. Verify that an alert is displayed in the OperatorConsole

### Test 2.2: Collision Prediction

**Purpose:** Verify that the system predicts future separation violations.

**Input:**
Create file `/tmp/atc/plane_input.txt` with:
```
0 1 10000 20000 15000 100 50 0
0 2 20000 30000 15000 -100 -50 0
```

**Expected Output:**
- System calculates that planes will violate separation in the future
- Predicted violation time is calculated
- Alert message is sent to OperatorConsole

**Testing Procedure:**
1. Start the system with the input file
2. Check computer_system.log for future violation prediction
3. Verify that a future violation alert is displayed

### Test 2.3: Congestion Parameter Adjustment

**Purpose:** Verify that changing the congestion parameter affects collision prediction.

**Input:**
- Same as Test 2.2, plus operator command:
```
update_congestion 30
```

**Expected Output:**
- System updates congestion degree parameter
- Future violation checks use the new prediction time
- Different alerts may be generated based on the new parameter

**Testing Procedure:**
1. Start the system with Test 2.2 input
2. Enter the command in the OperatorConsole
3. Check computer_system.log for updated prediction behavior

## 3. Logging and History Tests

### Test 3.1: Airspace History Logging

**Purpose:** Verify that the system logs airspace history correctly.

**Input:** Same as Test 1.2

**Expected Output:**
- AirspaceLogger receives log requests every 20 seconds
- History file contains timestamped entries of aircraft positions and velocities

**Testing Procedure:**
1. Start the system with Test 1.2 input
2. Let it run for at least 40 seconds
3. Check airspacelog.txt for history entries

### Test 3.2: Command Logging

**Purpose:** Verify that operator commands are logged correctly.

**Input:**
After system startup, enter multiple commands:
```
show_plane 1
set_velocity 1 200 100 0
update_congestion 60
```

**Expected Output:**
- All commands are logged in commandlog.txt with timestamps

**Testing Procedure:**
1. Start the system
2. Enter the commands in the OperatorConsole
3. Check commandlog.txt to verify all commands are logged

## 4. Performance Tests

### Test 4.1: Low Load Test

**Purpose:** Verify system performance under low load.

**Input:**
Create file with 3 aircraft:
```
0 1 10000 20000 15000 100 50 0
5 2 30000 40000 18000 -75 25 0
10 3 80000 10000 20000 -50 100 0
```

**Expected Output:**
- System processes all aircraft with minimal delays
- Log messages show regular timing
- No performance degradation

**Testing Procedure:**
1. Start the system with the input file
2. Let it run for 2 minutes
3. Check logs for timing information

### Test 4.2: Medium Load Test

**Purpose:** Verify system performance under medium load.

**Input:**
Create file with 5-7 aircraft with varied positions and velocities.

**Expected Output:**
- System processes all aircraft with acceptable delays
- Log messages show consistent timing
- Minor performance impact may be observed

**Testing Procedure:**
1. Start the system with the input file
2. Let it run for 2 minutes
3. Check logs for timing information

### Test 4.3: High Load Test

**Purpose:** Verify system performance under high load.

**Input:**
Create file with MAX_PLANES (10) aircraft with varied positions and velocities.

**Expected Output:**
- System processes all aircraft with some delay
- Log messages may show timing variations
- Performance impact should be noticeable but manageable

**Testing Procedure:**
1. Start the system with the input file
2. Let it run for 2 minutes
3. Check logs for timing information

### Test 4.4: Overload Test

**Purpose:** Verify system behavior under excessive load.

**Input:**
Create file with more than MAX_PLANES aircraft.

**Expected Output:**
- System should handle MAX_PLANES aircraft
- Excess aircraft should be ignored or queued
- Log messages should indicate the overload condition
- System should remain stable

**Testing Procedure:**
1. Start the system with the input file
2. Let it run for 2 minutes
3. Check logs for error or warning messages related to capacity
4. Verify system stability

## 5. Fault Tolerance Tests

### Test 5.1: Shared Memory Resilience

**Purpose:** Verify system resilience to shared memory access issues.

**Testing Procedure:**
1. Start the system
2. Manually corrupt a shared memory segment using external tools
3. Observe how the system detects and recovers from the issue

### Test 5.2: Process Failure Recovery

**Purpose:** Verify system behavior when a subsystem crashes.

**Testing Procedure:**
1. Start the system
2. Manually kill one of the subsystem processes (e.g., DataDisplay)
3. Observe how the system detects and handles the failure

### Test 5.3: Resource Exhaustion

**Purpose:** Verify system behavior under resource constraints.

**Testing Procedure:**
1. Start the system
2. Create conditions to exhaust resources (e.g., fill disk space for logs)
3. Observe system behavior and recovery mechanisms

## 6. Sample Input Files

### Sample 1: Basic Configuration
```
0 1 10000 20000 15000 100 50 0
5 2 30000 40000 18000 -75 25 0
10 3 80000 10000 20000 -50 100 0
```

### Sample 2: Collision Course
```
0 1 10000 20000 15000 100 50 0
0 2 40000 50000 15000 -100 -50 0
```

### Sample 3: High Load
```
0 1 10000 20000 15000 100 50 0
0 2 30000 40000 18000 -75 25 0
0 3 80000 10000 20000 -50 100 0
0 4 15000 60000 17000 90 -30 0
0 5 70000 45000 19000 -85 -45 0
0 6 45000 75000 16000 -30 -60 0
0 7 25000 85000 18500 70 -40 0
0 8 55000 15000 20000 -60 80 0
0 9 90000 30000 17500 -90 60 0
0 10 60000 50000 19500 40 30 0
```