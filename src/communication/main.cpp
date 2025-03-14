#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include "../../include/shared_memory.h"

int main() {
    SharedData* data;
    init_shared_memory(data);
    sem_t* cmd_sem = sem_open("/command_sem", O_CREAT, 0644, 0);
    sem_t* aircraft_sem = sem_open("/aircraft_sem", O_CREAT, 0644, 1);

    while (true) {
        sem_wait(cmd_sem);
        sem_wait(aircraft_sem);
        if (data->command_front != data->command_rear) {
            Command cmd = data->commands[data->command_front];
            int id = cmd.aircraft_id;
            if (data->aircrafts[id].active) {
                if (cmd.type == 0) {  // Speed
                    data->aircrafts[id].speed_x = cmd.data[0];
                    data->aircrafts[id].speed_y = cmd.data[1];
                    data->aircrafts[id].speed_z = cmd.data[2];
                } else if (cmd.type == 1) {  // Altitude
                    data->aircrafts[id].z = cmd.data[0];
                    data->aircrafts[id].speed_z = 0;
                } else if (cmd.type == 2) {  // Position
                    data->aircrafts[id].x = cmd.data[0];
                    data->aircrafts[id].y = cmd.data[1];
                    data->aircrafts[id].z = cmd.data[2];
                    data->aircrafts[id].speed_x = 0;
                    data->aircrafts[id].speed_y = 0;
                    data->aircrafts[id].speed_z = 0;
                }
                printf("Applied command to Aircraft %d\n", id);
            }
            data->command_front = (data->command_front + 1) % MAX_COMMANDS;
        }
        sem_post(aircraft_sem);
    }

    return 0;
}
