#include "Plane.h"
#include <iostream>
#include <mutex>
#include <unistd.h> 
#include "utils.h"

Plane::Plane(int pid, double px, double py, double pz,
             double vx_, double vy_, double vz_)
    : id(pid), 
      x(px), y(py), z(pz),
      vx(vx_), vy(vy_), vz(vz_), 
      lastUpdateTime(0.0),
      running(false)
{
    if (!isPositionWithinBounds(x, y, z)) {
        logPlaneMessage(id, "Started out-of-bounds, clamping to boundaries", LOG_WARNING);
        
        // Clamp to boundaries
        if (x < AIRSPACE_X_MIN) x = AIRSPACE_X_MIN;
        else if (x > AIRSPACE_X_MAX) x = AIRSPACE_X_MAX;
        
        if (y < AIRSPACE_Y_MIN) y = AIRSPACE_Y_MIN;
        else if (y > AIRSPACE_Y_MAX) y = AIRSPACE_Y_MAX;
        
        if (z < AIRSPACE_Z_MIN) z = AIRSPACE_Z_MIN;
        else if (z > AIRSPACE_Z_MAX) z = AIRSPACE_Z_MAX;
        
        logPlaneMessage(id, "Position clamped to (" + 
                           std::to_string(x) + "," + 
                           std::to_string(y) + "," + 
                           std::to_string(z) + ")");
    }
    
    logPlaneMessage(id, "Plane initialized at (" + 
                       std::to_string(x) + "," + 
                       std::to_string(y) + "," + 
                       std::to_string(z) + ") with velocity (" +
                       std::to_string(vx) + "," + 
                       std::to_string(vy) + "," + 
                       std::to_string(vz) + ")");
}

Plane::~Plane() {
    stop();
    logPlaneMessage(id, "Plane destroyed");
}

void Plane::start(double startTime) {
    // Guard against restarting an already running plane.
    if (running) {
        logPlaneMessage(id, "Plane already running, start() ignored", LOG_DEBUG);
        return;
    }
    running = true;
    planeThread = std::thread(&Plane::runPlaneProcess, this, startTime);
    logPlaneMessage(id, "Plane thread started at time " + std::to_string(startTime));
}

void Plane::stop() {
    if (running) {
        running = false;
        if (planeThread.joinable()) {
            planeThread.join();
            logPlaneMessage(id, "Plane thread stopped");
        }
    }
}

void Plane::runPlaneProcess(double startTime) {
    lastUpdateTime = startTime;
    
    logPlaneMessage(id, "Plane process running, starting at time " + std::to_string(startTime));
    
    while (running) {
        updatePosition(lastUpdateTime + 1.0);
        
        usleep(1000000); // 1 second
    }
}

void Plane::updatePosition(double currentTime) {
    double dt = currentTime - lastUpdateTime;
    if (dt >= 1.0) {
        // Calculate new position
        double newX, newY, newZ;
        
        {
            std::lock_guard<std::mutex> lock(positionMutex);
            newX = x + vx * dt;
            newY = y + vy * dt;
            newZ = z + vz * dt;
        }
        
        // Check for airspace boundary crossing
        bool wasBounded = isPositionWithinBounds(x, y, z);
        bool willBeBounded = isPositionWithinBounds(newX, newY, newZ);
        
        std::lock_guard<std::mutex> lock(positionMutex);
        
        if (!wasBounded && willBeBounded) {
            logPlaneMessage(id, "Entering airspace");
        } else if (wasBounded && !willBeBounded) {
            logPlaneMessage(id, "Exiting airspace", LOG_WARNING);
            
            // Calculate intersection point with boundary
            double boundaryX = x, boundaryY = y, boundaryZ = z;
            
            if (newX < AIRSPACE_X_MIN) boundaryX = AIRSPACE_X_MIN;
            else if (newX > AIRSPACE_X_MAX) boundaryX = AIRSPACE_X_MAX;
            
            if (newY < AIRSPACE_Y_MIN) boundaryY = AIRSPACE_Y_MIN;
            else if (newY > AIRSPACE_Y_MAX) boundaryY = AIRSPACE_Y_MAX;
            
            if (newZ < AIRSPACE_Z_MIN) boundaryZ = AIRSPACE_Z_MIN;
            else if (newZ > AIRSPACE_Z_MAX) boundaryZ = AIRSPACE_Z_MAX;
            
            // Use boundary point as final position
            x = boundaryX;
            y = boundaryY;
            z = boundaryZ;
            
            // Stop the plane at the boundary
            vx = vy = vz = 0.0;
            
            logPlaneMessage(id, "Now at boundary (" + 
                               std::to_string(x) + "," + 
                               std::to_string(y) + "," + 
                               std::to_string(z) + ") with zero velocity");
        } else {
            // Normal update within bounds
            x = newX;
            y = newY;
            z = newZ;
            
            logPlaneMessage(id, "Position updated to (" + 
                               std::to_string(x) + "," + 
                               std::to_string(y) + "," + 
                               std::to_string(z) + ")", LOG_DEBUG);
        }
        
        lastUpdateTime = currentTime;
    }
}

// Thread-safe getters
double Plane::getX() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return x;
}

double Plane::getY() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return y;
}

double Plane::getZ() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return z;
}

double Plane::getVx() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return vx;
}

double Plane::getVy() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return vy;
}

double Plane::getVz() const {
    std::lock_guard<std::mutex> lock(positionMutex);
    return vz;
}

void Plane::setVelocity(double vx_, double vy_, double vz_) {
    std::lock_guard<std::mutex> lock(positionMutex);
    vx = vx_;
    vy = vy_;
    vz = vz_;
    
    logPlaneMessage(id, "Velocity updated to (" + 
                       std::to_string(vx) + "," + 
                       std::to_string(vy) + "," + 
                       std::to_string(vz) + ")");
}

void Plane::setPosition(double px, double py, double pz) {
    std::lock_guard<std::mutex> lock(positionMutex);
    x = px;
    y = py;
    z = pz;
    
    logPlaneMessage(id, "Position manually set to (" + 
                       std::to_string(x) + "," + 
                       std::to_string(y) + "," + 
                       std::to_string(z) + ")");
}