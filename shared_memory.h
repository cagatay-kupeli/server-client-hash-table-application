#ifndef SERVER_SHARED_MEMORY_H
#define SERVER_SHARED_MEMORY_H

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

#define BUFFER_SIZE 20
#define SHM_SIZE sizeof(Request) + BUFFER_SIZE + sizeof(size_t) + sizeof(size_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) + sizeof(pthread_cond_t) + sizeof(bool)

struct SharedMemory {
    Request requests[BUFFER_SIZE];
    size_t read_index = 0;
    size_t write_index = 0;
    pthread_mutex_t mutex;
    pthread_cond_t is_not_full;
    pthread_cond_t is_not_empty;
    bool is_running;
};

void initialize_mutex(SharedMemory *shared_memory) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_memory->mutex, &mutex_attr);
}

void initialize_conditions(SharedMemory *shared_memory) {
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_memory->is_not_full, &cond_attr);
    pthread_cond_init(&shared_memory->is_not_empty, &cond_attr);
}

void clean_up_mutex(SharedMemory *shared_memory) {
    pthread_mutex_destroy(&shared_memory->mutex);
    pthread_cond_destroy(&shared_memory->is_not_full);
    pthread_cond_destroy(&shared_memory->is_not_empty);
}

#endif //SERVER_SHARED_MEMORY_H
