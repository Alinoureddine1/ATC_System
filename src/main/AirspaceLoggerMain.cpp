#include "AirspaceLogger.h"
#include <iostream>

int main(){
    std::cout<<"[AirspaceLogger] subsystem starting\n";
    AirspaceLogger al;
    al.run();
    return 0;
}
