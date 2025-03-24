#ifndef PLANE_H
#define PLANE_H

#include "utils.h"
#include "commandCodes.h"

/** Represents a single plane. Updated by Radar every second. */
class Plane {
private:
    int id;
    double x, y, z;
    double vx, vy, vz;
    double lastUpdateTime;

public:
    Plane(int pid, double x, double y, double z,
          double vx, double vy, double vz);

    void updatePosition(double currentTime);

    int    getId() const   { return id; }
    double getX()  const   { return x;  }
    double getY()  const   { return y;  }
    double getZ()  const   { return z;  }
    double getVx() const   { return vx; }
    double getVy() const   { return vy; }
    double getVz() const   { return vz; }

    void setVelocity(double vx, double vy, double vz);
    void setPosition(double x, double y, double z);
};

#endif // PLANE_H
