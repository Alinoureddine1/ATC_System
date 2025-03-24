#include "Plane.h"
#include <iostream>

Plane::Plane(int pid, double px, double py, double pz,
             double vx, double vy, double vz)
    : id(pid), x(px), y(py), z(pz),
      vx(vx), vy(vy), vz(vz), lastUpdateTime(0.0)
{
    if (!isPositionWithinBounds(x, y, z)) {
        std::cerr << "Plane " << id << " started out-of-bounds, clamping.\n";
        if (x < AIRSPACE_X_MIN) x = AIRSPACE_X_MIN;
        else if (x > AIRSPACE_X_MAX) x = AIRSPACE_X_MAX;
        if (y < AIRSPACE_Y_MIN) y = AIRSPACE_Y_MIN;
        else if (y > AIRSPACE_Y_MAX) y = AIRSPACE_Y_MAX;
        if (z < AIRSPACE_Z_MIN) z = AIRSPACE_Z_MIN;
        else if (z > AIRSPACE_Z_MAX) z = AIRSPACE_Z_MAX;
    }
}

void Plane::updatePosition(double currentTime) {
    double dt = currentTime - lastUpdateTime;
    if (dt >= 1.0) {
        x += vx * dt;
        y += vy * dt;
        z += vz * dt;

        if (!isPositionWithinBounds(x, y, z)) {
            // clamp & stop
            if (x < AIRSPACE_X_MIN) x = AIRSPACE_X_MIN; else if (x > AIRSPACE_X_MAX) x = AIRSPACE_X_MAX;
            if (y < AIRSPACE_Y_MIN) y = AIRSPACE_Y_MIN; else if (y > AIRSPACE_Y_MAX) y = AIRSPACE_Y_MAX;
            if (z < AIRSPACE_Z_MIN) z = AIRSPACE_Z_MIN; else if (z > AIRSPACE_Z_MAX) z = AIRSPACE_Z_MAX;
            vx = vy = vz = 0.0;
            std::cout << printTimeStamp() << " Plane " << id << " hit boundary.\n";
        }
        lastUpdateTime = currentTime;
    }
}

void Plane::setVelocity(double vx_, double vy_, double vz_) {
    vx = vx_;
    vy = vy_;
    vz = vz_;
}

void Plane::setPosition(double px, double py, double pz) {
    x = px; y = py; z = pz;
}
