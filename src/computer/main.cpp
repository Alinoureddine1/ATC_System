#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <cmath>
#include "../../include/shared_memory.h"

void check_safety(SharedData* data) {
    sem_t* sem = sem_open("/aircraft_sem", O_CREAT, 0644, 1);
    sem_wait(sem);
    for (int i = 0; i < MAX_AIRCRAFT; i++) {
        if (!data->aircrafts[i].active) continue;
        for (int j = i + 1; j < MAX_AIRCRAFT; j++) {
            if (!data->aircrafts[j].active) continue;
            double fx1 = data->aircrafts[i].x + data->aircrafts[i].speed_x * data->safety_n;
            double fy1 = data->aircrafts[i].y + data->aircrafts[i].speed_y * data->safety_n;
            double fz1 = data->aircrafts[i].z + data->aircrafts[i].speed_z * data->safety_n;
            double fx2 = data->aircrafts[j].x + data->aircrafts[j].speed_x * data->safety_n;
            double fy2 = data->aircrafts[j].y + data->aircrafts[j].speed_y * data->safety_n;
            double fz2 = data->aircrafts[j].z + data->aircrafts[j].speed_z * data->safety_n;
            double h_dist = sqrt(pow(fx1 - fx2, 2) + pow(fy1 - fy2, 2));
            double v_dist = fabs(fz1 - fz2);
            if (h_dist < 3000 && v_dist < 1000 && data->safety_n <= 120) {
                data->alarm_active = true;
                printf("ALARM: Aircraft %d and %d too close at t+%d!\n",
                       data->aircrafts[i].id, data->aircrafts[j].id, data->safety_n);
            }
        }
    }
    sem_post(sem);
}

int main() {
    SharedData* data;
    init_shared_memory(data);

    while (true) {
        sleep(data->safety_n);
        check_safety(data);
    }
    return 0;
}
