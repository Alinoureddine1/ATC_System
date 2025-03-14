#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include "../../include/shared_memory.h"

int main() {
    SharedData* data;
    init_shared_memory(data);
    sem_t* sem = sem_open("/aircraft_sem", O_CREAT, 0644, 1);

    while (true) {
        sleep(1);
        sem_wait(sem);
        printf("Radar Scan:\n");
        for (int i = 0; i < MAX_AIRCRAFT; i++) {
            if (data->aircrafts[i].active) {
                printf("Aircraft %d: (%f, %f, %f), Speed: (%f, %f, %f)\n",
                       data->aircrafts[i].id, data->aircrafts[i].x, data->aircrafts[i].y,
                       data->aircrafts[i].z, data->aircrafts[i].speed_x,
                       data->aircrafts[i].speed_y, data->aircrafts[i].speed_z);
            }
        }
        sem_post(sem);
    }

    sem_close(sem);
    return 0;
}
