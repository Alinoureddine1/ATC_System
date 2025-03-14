#include "../include/shared_memory.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

void init_shared_memory(SharedData*& data) {
    int fd = shm_open("/atc_shm", O_CREAT | O_RDWR, 0666);
    if (fd == -1) { perror("shm_open failed"); exit(1); }
    ftruncate(fd, sizeof(SharedData));
    data = static_cast<SharedData*>(mmap(NULL, sizeof(SharedData),
                                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (data == MAP_FAILED) { perror("mmap failed"); exit(1); }
    if (data->num_aircraft == 0) {  // First process initializes
        for (int i = 0; i < MAX_AIRCRAFT; i++) data->aircrafts[i].active = false;
        data->command_front = 0;
        data->command_rear = 0;
        data->num_aircraft = 0;
        data->safety_n = 5;
        data->alarm_active = false;
        data->log_buffer[0] = '\0';
    }
    close(fd);
}
