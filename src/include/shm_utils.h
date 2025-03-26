#ifndef SHM_UTILS_H
#define SHM_UTILS_H

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <functional>
#include <string.h>   
#include <errno.h>   
#include "utils.h"
#include <mutex>

/**
 * Helper function to safely open and map shared memory
 * @param name Shared memory name
 * @param size Size to allocate/map
 * @param flags Open flags
 * @param createIfMissing Whether to create shm if it doesn't exist
 * @param processor Function to process the mapped memory
 * @param maxRetries Maximum number of retry attempts
 * @return true if successful, false otherwise
 */

static std::mutex shm_mutex;
template<typename T>
bool accessSharedMemory(const std::string& name, 
                        size_t size,
                        int flags,
                        bool createIfMissing,
                        std::function<void(T*)> processor,
                        int maxRetries = 5) {  // Increased default retries
    std::string subsystem = "SharedMemory";
    std::lock_guard<std::mutex> lock(shm_mutex);

    
    // Ensure log directory exists
    ensureLogDirectories();
    
    for (int retry = 0; retry < maxRetries; retry++) {
        int shm_fd;
        
        if (createIfMissing) {
            shm_fd = shm_open(name.c_str(), flags | O_CREAT, 0666);
        } else {
            shm_fd = shm_open(name.c_str(), flags, 0666);
        }
        
        if (shm_fd == -1) {
            logSystemMessage("Failed to open shared memory " + name + ": " + 
                           std::string(strerror(errno)) + " (retry " + 
                           std::to_string(retry + 1) + "/" + 
                           std::to_string(maxRetries) + ")", LOG_WARNING);
            
            if (retry < maxRetries - 1) {
                usleep(500000); // 500ms before retry - increased delay
                continue;
            }
            return false;
        }
        
        // Set size if creating or writing
        if ((flags & O_RDWR) && createIfMissing) {
            if (ftruncate(shm_fd, size) == -1) {
                logSystemMessage("Failed to set size of shared memory " + name + 
                               ": " + std::string(strerror(errno)), LOG_ERROR);
                close(shm_fd);
                return false;
            }
        }
        
        // Map the memory
        int prot = (flags & O_RDWR) ? (PROT_READ | PROT_WRITE) : PROT_READ;
        T* ptr = static_cast<T*>(mmap(nullptr, size, prot, MAP_SHARED, shm_fd, 0));
        
        if (ptr == MAP_FAILED) {
            logSystemMessage("Failed to map shared memory " + name + ": " + 
                           std::string(strerror(errno)), LOG_ERROR);
            close(shm_fd);
            
            if (retry < maxRetries - 1) {
                usleep(500000); // 500ms before retry - increased delay
                continue;
            }
            return false;
        }
        
        try {
            // Process the mapped memory
            processor(ptr);
        } catch (const std::exception& e) {
            logSystemMessage("Exception while processing shared memory " + name + 
                           ": " + e.what(), LOG_ERROR);
            munmap(ptr, size);
            close(shm_fd);
            return false;
        }
        
        // Clean up
        munmap(ptr, size);
        close(shm_fd);
        return true;
    }
    
    return false;
}

/**
 * Helper to initialize shared memory for channel IDs
 * @param shmName Name of the shared memory segment
 * @param initFunc Function to initialize the channel IDs
 * @return true if successful, false otherwise
 */
inline bool initializeChannelIdsSharedMemory(const std::string& shmName, 
                                          std::function<void(ChannelIds*)> initFunc) {
    return accessSharedMemory<ChannelIds>(
        shmName,
        sizeof(ChannelIds),
        O_RDWR,
        true, 
        [&initFunc](ChannelIds* channels) {
            initFunc(channels);
        },
        5  
    );
}

#endif // SHM_UTILS_H