#ifndef PLANE_H
#define PLANE_H

#include "utils.h"
#include "commandCodes.h"
#include <atomic>
#include <thread>
#include <mutex>

/** Represents a single plane. Updated by Radar every second. */
class Plane {
private:
    int id;
    double x, y, z;
    double vx, vy, vz;
    double lastUpdateTime;
    
    // Thread safety and threading support
    mutable std::mutex positionMutex;
    std::atomic<bool> running;
    std::thread planeThread;
    
    void runPlaneProcess(double startTime);

public:
    Plane(int pid, double x, double y, double z,
          double vx, double vy, double vz);
    ~Plane();

    void updatePosition(double currentTime);
    void start(double startTime);
    void stop();

    // Thread-safe getters
    int    getId() const   { return id; }
    
    // These getters need mutex protection
    double getX() const;
    double getY() const;   
    double getZ() const;   
    double getVx() const;  
    double getVy() const;  
    double getVz() const;  

    // Thread-safe position and velocity updates
    void setVelocity(double vx, double vy, double vz);
    void setPosition(double x, double y, double z);
};

#endif // PLANE_H