#ifndef COMPUTER_SYSTEM_H
#define COMPUTER_SYSTEM_H

#include <vector>
#include "Radar.h"
#include "commandCodes.h"

class ComputerSystem {
private:
    Radar& radar; 
    double predictionTime; 
    const double MIN_VERTICAL_SEPARATION = 1000.0; // Constraint in ft
    const double MIN_HORIZONTAL_SEPARATION = 3000.0; // Constraint in ft

    bool checkSeparation(const Position& pos1, const Position& pos2) const;
    bool predictSeparation(const Position& pos1, const Velocity& vel1, 
                          const Position& pos2, const Velocity& vel2) const;

public:
    ComputerSystem(Radar& radar, double predictionTime = 120.0);
    void update(double currentTime);
    std::vector<Position> getDisplayData() const; 
};

#endif // COMPUTER_SYSTEM_H