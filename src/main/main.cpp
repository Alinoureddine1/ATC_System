#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <errno.h>
#include "utils.h"
#include "Plane.h"

int main(int argc, char* argv[]) {
    std::string mode;
    std::cout << printTimeStamp() << " Enter mode ('write' or 'run'): ";
    std::cin >> mode;

    if (mode == "write") {
        std::string logDir = "/tmp/logs"; // will change 
        std::cout << printTimeStamp() << " Attempting to create directory: " << logDir << "\n";

        
        errno = 0;
        if (mkdir(logDir.c_str(), 0700) == -1 && errno != EEXIST) {
            std::cerr << printTimeStamp() << " Error creating directory " << logDir 
                      << ": " << strerror(errno) << " (errno: " << errno << ")\n";
            return 1;
        }

        int fd;
        const char* filenames[] = {"lowload.txt", "medload.txt", "highload.txt"};
        const char* data[] = {
            "0 1 1000 1000 15000 50 0 0\n",
            "0 2 2000 2000 16000 60 0 0\n0 3 2500 2500 16000 -50 0 0\n",
            "0 4 3000 3000 17000 70 0 0\n0 5 3500 3500 17000 60 0 0\n0 6 4000 4000 17000 -60 0 0\n"
        };

        for (int i = 0; i < 3; ++i) {
            std::string filepath = logDir + "/" + filenames[i];
            errno = 0; 
            fd = open(filepath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd == -1) {
                std::cerr << printTimeStamp() << " Error opening " << filepath 
                          << ": " << strerror(errno) << " (errno: " << errno << ")\n";
                continue;
            }
            errno = 0; // Reset errno before write
            if (write(fd, data[i], strlen(data[i])) == -1) {
                std::cerr << printTimeStamp() << " Error writing to " << filepath 
                          << ": " << strerror(errno) << " (errno: " << errno << ")\n";
            }
            close(fd);
        }

        std::cout << printTimeStamp() << " Test files written to " << logDir << "\n";
    } else if (mode == "run") {
        std::cout << printTimeStamp() << " Starting ATC simulation\n";

        std::vector<Plane> planes;
        planes.emplace_back(1, 1000.0, 1000.0, 15000.0, 50.0, 0.0, 0.0);
        planes.emplace_back(2, 2000.0, 2000.0, 16000.0, 0.0, 60.0, 0.0);
        planes.emplace_back(3, 2500.0, 2500.0, 16000.0, -50.0, 0.0, 0.0);

        for (double t = 0.0; t <= 10.0; t += 1.0) {
            std::cout << printTimeStamp() << " Time: " << t << "s\n";
            for (auto& plane : planes) {
                plane.updatePosition(t);
                std::cout << "  Plane " << plane.getId() << ": Position (" 
                          << plane.getX() << ", " << plane.getY() << ", " << plane.getZ() 
                          << "), Velocity (" << plane.getVx() << ", " << plane.getVy() 
                          << ", " << plane.getVz() << ")\n";
            }
            sleep(1);
        }
    } else {
        std::cout << printTimeStamp() << " Invalid mode. Use 'write' or 'run'.\n";
    }

    return 0;
}