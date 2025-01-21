#ifndef SERVER_SHARED_MEMORY_H
#define SERVER_SHARED_MEMORY_H

// Constant for the shared memory segment name
#define SHM_NAME "/hash_table_shm"

#include <atomic>
#include <string>
#include <array>
#include <shared_mutex>
#include <thread>
#include <iostream>
#include <utility>
#include <functional>

#include "hash_map.h"


// Structure to represents a request that will be stored in the shared memory
struct Request {
    enum class Operation {
        INSERT, READ, DELETE
    } operation;
    std::string key;
    std::string value;

    Request(Operation operation, std::string key, std::string value) : operation(operation), key(std::move(key)),
                                                                       value(std::move(value)) {}

    Request(Operation operation, std::string key) : operation(operation), key(std::move(key)) {}
};

// Constant for the buffer size (i.e., the maximum number of requests that can be queued in shared memory
#define BUFFER_SIZE 20

// Constant for the size of the shared memory segment
#define SHM_SIZE sizeof(Request) + BUFFER_SIZE + sizeof(size_t) + sizeof(size_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) + sizeof(pthread_cond_t) + sizeof(bool)

// Structure to represents shared memory segment
struct SharedMemory {
    // A circular buffer to store requests
    Request requests[BUFFER_SIZE];

    // Indices to track read and write positions in the buffer
    size_t read_index = 0;
    size_t write_index = 0;

    // Mutex to protect shared memory resources
    pthread_mutex_t mutex;

    // Condition variables to signal when buffer is not full or not empty
    pthread_cond_t is_not_full;
    pthread_cond_t is_not_empty;

    // Boolean flag to indicate if a server is running
    bool is_running;
};

// Helper function to initialize the mutex for the shared memory
void initialize_mutex(SharedMemory *shared_memory) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_memory->mutex, &mutex_attr);
}

// Helper function to initialize the conditional variables for the shared memory
void initialize_conditions(SharedMemory *shared_memory) {
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_memory->is_not_full, &cond_attr);
    pthread_cond_init(&shared_memory->is_not_empty, &cond_attr);
}

// Helper function to clean up and destroy the mutex and conditional variables
void clean_up_mutex(SharedMemory *shared_memory) {
    pthread_mutex_destroy(&shared_memory->mutex);
    pthread_cond_destroy(&shared_memory->is_not_full);
    pthread_cond_destroy(&shared_memory->is_not_empty);
}

#endif //SERVER_SHARED_MEMORY_H
