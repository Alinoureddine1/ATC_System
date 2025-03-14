#ifndef AIRCRAFT_DATA_H
#define AIRCRAFT_DATA_H

const int MAX_AIRCRAFT = 50;
const int MAX_COMMANDS = 20;

struct AircraftData {
    int id;
    double x, y, z;
    double speed_x, speed_y, speed_z;
    bool active;  
};

struct Command {
    int type;  // 0=speed, 1=altitude, 2=position
    int aircraft_id;
    double data[3];
};

#endif