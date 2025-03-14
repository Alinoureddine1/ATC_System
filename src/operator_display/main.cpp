#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include "../../include/shared_memory.h"

int main() {
    SharedData* data;
    init_shared_memory(data);
    sem_t* cmd_sem = sem_open("/command_sem", O_CREAT, 0644, 0);
    sem_t* aircraft_sem = sem_open("/aircraft_sem", O_CREAT, 0644, 1);
    FILE* airspace_log = fopen("/tmp/airspace_history.log", "a");
    FILE* cmd_log = fopen("/tmp/operator_commands.log", "a");
    int display_count = 0;

    while (true) {
        if (display_count % 5 == 0) {  // 5 seconds
            sem_wait(aircraft_sem);
            printf("\nAirspace View (Time %d):\n", display_count);
            for (int i = 0; i < MAX_AIRCRAFT; i++) {
                if (data->aircrafts[i].active) {
                    printf("Aircraft %d: (%f, %f, %f)\n", data->aircrafts[i].id,
                           data->aircrafts[i].x, data->aircrafts[i].y, data->aircrafts[i].z);
                }
            }
            if (data->alarm_active) {
                printf("ALARM ACTIVE\n");
                data->alarm_active = false;
            }
            if (display_count % 20 == 0) {  // log every 20 seconds
                fprintf(airspace_log, "Time %d: ", display_count);
                for (int i = 0; i < MAX_AIRCRAFT; i++) {
                    if (data->aircrafts[i].active) {
                        fprintf(airspace_log, "ID%d:(%f,%f,%f) ",
                                data->aircrafts[i].id, data->aircrafts[i].x,
                                data->aircrafts[i].y, data->aircrafts[i].z);
                    }
                }
                fprintf(airspace_log, "\n");
                fflush(airspace_log);
            }
            sem_post(aircraft_sem);
        }

        std::string input;
        std::cout << "Command (e.g., 'speed 0 100 0 0', 'n 10'): ";
        std::getline(std::cin, input);
        sem_wait(aircraft_sem);
        if (input.find("speed") == 0) {
            Command cmd{0, 0, {0, 0, 0}};
            sscanf(input.c_str(), "speed %d %lf %lf %lf", &cmd.aircraft_id,
                   &cmd.data[0], &cmd.data[1], &cmd.data[2]);
            data->commands[data->command_rear] = cmd;
            data->command_rear = (data->command_rear + 1) % MAX_COMMANDS;
            sem_post(cmd_sem);
        } else if (input.find("altitude") == 0) {
            Command cmd{1, 0, {0, 0, 0}};
            sscanf(input.c_str(), "altitude %d %lf", &cmd.aircraft_id, &cmd.data[0]);
            data->commands[data->command_rear] = cmd;
            data->command_rear = (data->command_rear + 1) % MAX_COMMANDS;
            sem_post(cmd_sem);
        } else if (input.find("position") == 0) {
            Command cmd{2, 0, {0, 0, 0}};
            sscanf(input.c_str(), "position %d %lf %lf %lf", &cmd.aircraft_id,
                   &cmd.data[0], &cmd.data[1], &cmd.data[2]);
            data->commands[data->command_rear] = cmd;
            data->command_rear = (data->command_rear + 1) % MAX_COMMANDS;
            sem_post(cmd_sem);
        } else if (input.find("n") == 0) {
            int new_n;
            sscanf(input.c_str(), "n %d", &new_n);
            data->safety_n = new_n;
            printf("Safety interval set to %d seconds\n", new_n);
        }
        fprintf(cmd_log, "Time %d: %s\n", display_count, input.c_str());
        fflush(cmd_log);
        sem_post(aircraft_sem);

        sleep(1);
        display_count++;
    }

    fclose(airspace_log);
    fclose(cmd_log);
    return 0;
}
