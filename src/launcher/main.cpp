#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/shared_memory.h"

int main() {
    FILE* fp = fopen("../tests/input_files/aircraft_input.txt", "r");
    if (!fp) {
        perror("Failed to open input file");
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        int time, id;
        double x, y, z, sx, sy, sz;
        if (sscanf(line, "%d %d %lf %lf %lf %lf %lf %lf", &time, &id, &x, &y, &z, &sx, &sy, &sz) == 8) {
            sleep(time);  // Wait until entry time
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "./aircraft %d %lf %lf %lf %lf %lf %lf &",
                     id, x, y, z, sx, sy, sz);
            system(cmd);
            printf("Launched aircraft %d\n", id);
        }
    }

    fclose(fp);
    return 0;
}
