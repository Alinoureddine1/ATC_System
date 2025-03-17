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

    // Assignment operator to allow updates via commands
    Plane& operator=(const Plane& other) {
        if (this != &other) {
            this->id = other.id;
            this->x = other.x;
            this->y = other.y;
            this->z = other.z;
            this->vx = other.vx;
            this->vy = other.vy;
            this->vz = other.vz;
            this->lastUpdateTime = other.lastUpdateTime;
        }
        return *this;
    }
};

#endif // PLANE_H