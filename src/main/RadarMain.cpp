#include "Radar.h"
#include "Plane.h"
#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "[Radar] subsystem starting\n";

    std::vector<Plane> planes={
        Plane(1,10000.0,20000.0,5000.0,100.0,50.0,0.0),
        Plane(2,30000.0,40000.0,7000.0,-50.0,100.0,0.0)
    };

    Radar radar;
    radar.detectAircraft(planes);

    double t=0.0;
    while(true){
        radar.update(t);
        sleep(1);
        t+=1.0;
    }
    return 0;
}
