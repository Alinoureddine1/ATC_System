#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../include/shared_memory.h"

int main(int argc, char* argv[]) {
    if (argc < 8) {
        printf("Usage: %s id x y z sx sy sz\n", argv[0]);
        return 1;
    }

    SharedData* data;
    init_shared_memory(data);
    sem_t* sem = sem_open("/aircraft_sem", O_CREAT, 0644, 1);

    int id = atoi(argv[1]);
    sem_wait(sem);
    data->aircrafts[id].id = id;
    data->aircrafts[id].x = atof(argv[2]);
    data->aircrafts[id].y = atof(argv[3]);
    data->aircrafts[id].z = atof(argv[4]);
    data->aircrafts[id].speed_x = atof(argv[5]);
    data->aircrafts[id].speed_y = atof(argv[6]);
    data->aircrafts[id].speed_z = atof(argv[7]);
    data->aircrafts[id].active = true;
    data->num_aircraft++;
    sem_post(sem);

    while (true) {
        sleep(1);
        sem_wait(sem);
        if (!data->aircrafts[id].active) {
            sem_post(sem);
            break;
        }
        data->aircrafts[id].x += data->aircrafts[id].speed_x;
        data->aircrafts[id].y += data->aircrafts[id].speed_y;
        data->aircrafts[id].z += data->aircrafts[id].speed_z;
        // check airspace bounds (100,000 x 100,000 x 25,000 above 15,000 ft)
        if (data->aircrafts[id].x < 0 || data->aircrafts[id].x > 100000 ||
            data->aircrafts[id].y < 0 || data->aircrafts[id].y > 100000 ||
            data->aircrafts[id].z < 15000 || data->aircrafts[id].z > 40000) {
            data->aircrafts[id].active = false;
            data->num_aircraft--;
            printf("Aircraft %d exited airspace\n", id);
        }
        sem_post(sem);
    }

    sem_close(sem);
    if (data->num_aircraft == 0) {
        shm_unlink("/atc_shm");
        sem_unlink("/aircraft_sem");
    }
    munmap(data, sizeof(SharedData));
    return 0;
}
