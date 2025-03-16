#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

int main(int argc, char* argv[]) {
    std::string mode;
    std::cout << printTimeStamp() << " Enter mode ('write' or 'run'): ";
    std::cin >> mode;

    if (mode == "write") {
        int fd;
        mkdir("/logs", 0700);

        fd = open("/logs/lowload.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd != -1) {
            const char* data = "0 1 1000 1000 15000 50 0 0\n";
            write(fd, data, strlen(data));
            close(fd);
        }

        fd = open("/logs/medload.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd != -1) {
            const char* data = "0 2 2000 2000 16000 60 0 0\n0 3 2500 2500 16000 -50 0 0\n";
            write(fd, data, strlen(data));
            close(fd);
        }

        fd = open("/logs/highload.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd != -1) {
            const char* data = "0 4 3000 3000 17000 70 0 0\n0 5 3500 3500 17000 60 0 0\n0 6 4000 4000 17000 -60 0 0\n";
            write(fd, data, strlen(data));
            close(fd);
        }

        std::cout << printTimeStamp() << " Test files written to /logs\n";
    } else if (mode == "run") {
        std::cout << printTimeStamp() << " Starting ATC simulation (to be implemented)\n";
    } else {
        std::cout << printTimeStamp() << " Invalid mode. Use 'write' or 'run'.\n";
    }

    return 0;
}