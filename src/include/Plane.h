#ifndef PLANE_H
#define PLANE_H

#include "utils.h"

class Plane {
private:
    int id;
    double x, y, z;        // Position
    double vx, vy, vz;     // Velocity
    double lastUpdateTime; // Last update timestamp

public:
    Plane(int id, double x, double y, double z, double vx, double vy, double vz);
    void updatePosition(double currentTime);
    int getId() const { return id; }
    double getX() const { return x; }
    double getY() const { return y; }
    double getZ() const { return z; }
    double getVx() const { return vx; }
    double getVy() const { return vy; }
    double getVz() const { return vz; }
};

#endif // PLANE_H