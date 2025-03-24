ARTIFACT = ATC_System

# Build architecture/variant string 
PLATFORM ?= x86_64

# Build profile 
BUILD_PROFILE ?= debug

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)

CC = qcc -Vgcc_nto$(PLATFORM)
CXX = q++ -Vgcc_nto$(PLATFORM)_cxx
LD = $(CXX)

# Include directories
QNX_INCLUDE = C:/Users/pc/qnx710/target/qnx7/x86_64/usr/include
INCLUDES = -I src/include -I $(QNX_INCLUDE)
LDFLAGS =

# Compiler flags for build profiles
CCFLAGS_release += -O2
CCFLAGS_debug += -g -O0 -fno-builtin

CCFLAGS_all += -Wall -fmessage-length=0
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))

# Source files
MAIN_SOURCES = \
    src/main/ATCController.cpp \
    src/main/RadarMain.cpp \
    src/main/ComputerSystemMain.cpp \
    src/main/OperatorConsoleMain.cpp \
    src/main/DataDisplayMain.cpp \
    src/main/CommunicationSystemMain.cpp \
    src/main/AirspaceLoggerMain.cpp

SUBSYSTEM_SOURCES = \
    src/subsystems/Radar.cpp \
    src/subsystems/ComputerSystem.cpp \
    src/subsystems/OperatorConsole.cpp \
    src/subsystems/DataDisplay.cpp \
    src/subsystems/CommunicationSystem.cpp \
    src/subsystems/AirspaceLogger.cpp \
    src/subsystems/Plane.cpp

# Object files
MAIN_OBJS = $(patsubst src/main/%.cpp,$(OUTPUT_DIR)/main/%.o,$(MAIN_SOURCES))
SUBSYSTEM_OBJS = $(patsubst src/subsystems/%.cpp,$(OUTPUT_DIR)/subsystems/%.o,$(SUBSYSTEM_SOURCES))

# Executables
EXECUTABLES = \
    $(OUTPUT_DIR)/ATCController \
    $(OUTPUT_DIR)/Radar \
    $(OUTPUT_DIR)/ComputerSystem \
    $(OUTPUT_DIR)/OperatorConsole \
    $(OUTPUT_DIR)/DataDisplay \
    $(OUTPUT_DIR)/CommunicationSystem \
    $(OUTPUT_DIR)/AirspaceLogger

# Rules for building object files
$(OUTPUT_DIR)/main/%.o: src/main/%.cpp
	@mkdir -p $(OUTPUT_DIR)/main
	$(CXX) -c -o $@ $(INCLUDES) $(CCFLAGS_all) $<

$(OUTPUT_DIR)/subsystems/%.o: src/subsystems/%.cpp
	@mkdir -p $(OUTPUT_DIR)/subsystems
	$(CXX) -c -o $@ $(INCLUDES) $(CCFLAGS_all) $<

# 1) ATCController
$(OUTPUT_DIR)/ATCController: \
    $(OUTPUT_DIR)/main/ATCController.o \
    $(SUBSYSTEM_OBJS)
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 2) Radar
#   RadarMain references Radar (which references Plane).
$(OUTPUT_DIR)/Radar: \
    $(OUTPUT_DIR)/main/RadarMain.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/Plane.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 3) ComputerSystem
#   ComputerSystemMain references ComputerSystem.cpp, which also calls
#   Radar, CommunicationSystem, AirspaceLogger
$(OUTPUT_DIR)/ComputerSystem: \
    $(OUTPUT_DIR)/main/ComputerSystemMain.o \
    $(OUTPUT_DIR)/subsystems/ComputerSystem.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/CommunicationSystem.o \
    $(OUTPUT_DIR)/subsystems/AirspaceLogger.o \
    $(OUTPUT_DIR)/subsystems/Plane.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 4) OperatorConsole
#   OperatorConsoleMain references OperatorConsole, which references
#   CommunicationSystem, DataDisplay
$(OUTPUT_DIR)/OperatorConsole: \
    $(OUTPUT_DIR)/main/OperatorConsoleMain.o \
    $(OUTPUT_DIR)/subsystems/OperatorConsole.o \
    $(OUTPUT_DIR)/subsystems/DataDisplay.o \
    $(OUTPUT_DIR)/subsystems/CommunicationSystem.o \
    $(OUTPUT_DIR)/subsystems/ComputerSystem.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/AirspaceLogger.o \
    $(OUTPUT_DIR)/subsystems/Plane.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 5) DataDisplay
#   DataDisplayMain references DataDisplay, which references ComputerSystem,
#   which references Radar, CommunicationSystem, AirspaceLogger, Plane, etc.
$(OUTPUT_DIR)/DataDisplay: \
    $(OUTPUT_DIR)/main/DataDisplayMain.o \
    $(OUTPUT_DIR)/subsystems/DataDisplay.o \
    $(OUTPUT_DIR)/subsystems/ComputerSystem.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/CommunicationSystem.o \
    $(OUTPUT_DIR)/subsystems/AirspaceLogger.o \
    $(OUTPUT_DIR)/subsystems/Plane.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 6) CommunicationSystem
#   CommunicationSystemMain references CommunicationSystem, which references Plane.
$(OUTPUT_DIR)/CommunicationSystem: \
    $(OUTPUT_DIR)/main/CommunicationSystemMain.o \
    $(OUTPUT_DIR)/subsystems/CommunicationSystem.o \
    $(OUTPUT_DIR)/subsystems/Plane.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/ComputerSystem.o \
    $(OUTPUT_DIR)/subsystems/AirspaceLogger.o \
    $(OUTPUT_DIR)/subsystems/DataDisplay.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

# 7) AirspaceLogger
#   AirspaceLoggerMain references AirspaceLogger, which references Radar.
$(OUTPUT_DIR)/AirspaceLogger: \
    $(OUTPUT_DIR)/main/AirspaceLoggerMain.o \
    $(OUTPUT_DIR)/subsystems/AirspaceLogger.o \
    $(OUTPUT_DIR)/subsystems/Radar.o \
    $(OUTPUT_DIR)/subsystems/Plane.o \
    $(OUTPUT_DIR)/subsystems/CommunicationSystem.o \
    $(OUTPUT_DIR)/subsystems/ComputerSystem.o \
    $(OUTPUT_DIR)/subsystems/DataDisplay.o
	@mkdir -p $(OUTPUT_DIR)
	$(LD) -o $@ $^ $(LDFLAGS)

#-----------------------------------------------------------------
# Phony targets
#-----------------------------------------------------------------
all: $(EXECUTABLES)

clean:
	rm -fr $(OUTPUT_DIR)

rebuild: clean all
