#include "CommunicationSystem.h"
#include <iostream>

/**
 * CommunicationSystem reads /shm_commands in a loop, sending commands to planes.
 */

int main() {
    std::cout<<"[CommunicationSystem] subsystem starting\n";
    CommunicationSystem comm;
    comm.run(); // infinite loop reading commands
    return 0;
}
