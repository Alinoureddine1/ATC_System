#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "aircraft_data.h"

struct SharedData {
    AircraftData aircrafts[MAX_AIRCRAFT];
    Command commands[MAX_COMMANDS];
    int command_front, command_rear;
    int num_aircraft;
    int safety_n;
    bool alarm_active;
    char log_buffer[1024];
};
void init_shared_memory(SharedData*& data);
#endif
