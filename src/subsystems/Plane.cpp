#include "Plane.h"
#include <iostream>

Plane::Plane(int id, double x, double y, double z, double vx, double vy, double vz)
    : id(id), x(x), y(y), z(z), vx(vx), vy(vy), vz(vz), lastUpdateTime(0.0) {
    if (!isPositionWithinBounds(x, y, z)) {
        std::cerr << "Plane " << id << " initialized outside airspace bounds!\n";
        // Set to boundary if out of bounds
        this->x = (x < AIRSPACE_X_MIN) ? AIRSPACE_X_MIN : (x > AIRSPACE_X_MAX) ? AIRSPACE_X_MAX : x;
        this->y = (y < AIRSPACE_Y_MIN) ? AIRSPACE_Y_MIN : (y > AIRSPACE_Y_MAX) ? AIRSPACE_Y_MAX : y;
        this->z = (z < AIRSPACE_Z_MIN) ? AIRSPACE_Z_MIN : (z > AIRSPACE_Z_MAX) ? AIRSPACE_Z_MAX : z;
    }
}

void Plane::updatePosition(double currentTime) {
    double deltaTime = currentTime - lastUpdateTime;
    if (deltaTime >= 1.0) { 
        // Update position: new_pos = old_pos + velocity * time
        x += vx * deltaTime;
        y += vy * deltaTime;
        z += vz * deltaTime;

        // Check bounds and clamp if necessary
        if (!isPositionWithinBounds(x, y, z)) {
            x = (x < AIRSPACE_X_MIN) ? AIRSPACE_X_MIN : (x > AIRSPACE_X_MAX) ? AIRSPACE_X_MAX : x;
            y = (y < AIRSPACE_Y_MIN) ? AIRSPACE_Y_MIN : (y > AIRSPACE_Y_MAX) ? AIRSPACE_Y_MAX : y;
            z = (z < AIRSPACE_Z_MIN) ? AIRSPACE_Z_MIN : (z > AIRSPACE_Z_MAX) ? AIRSPACE_Z_MAX : z;
            // Stop the plane if it hits a boundary (velocity becomes 0 :( ))
            vx = vy = vz = 0.0;
            std::cout << printTimeStamp() << " Plane " << id << " hit airspace boundary at (" 
                      << x << ", " << y << ", " << z << "). Stopping.\n";
        }

        lastUpdateTime = currentTime;
    }
}