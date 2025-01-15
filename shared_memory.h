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
    enum class Operation { INSERT, READ, DELETE } operation;
    std::string key;
    std::string value;

    Request(Operation operation, std::string key, std::string  value) : operation(operation), key(std::move(key)), value(std::move(value)) {}

    Request(Operation operation, std::string key) : operation(operation), key(std::move(key)) {}
};

const size_t buffer_size = 10;

struct SharedMemory {
    std::array<Request, buffer_size> requests;
    size_t read_index = 0;
    size_t write_index = 0;
    bool is_empty = true;
    bool is_full = false;
};

#define SHM_SIZE sizeof(SharedMemory)

std::mutex shared_memory_mutex;

// Server
std::atomic<bool> is_running;

void sigint_handler(int signal) {
    if (signal == SIGINT) {
        is_running.store(false);
        std::printf("\nSIGINT received. Server is shutting down!\n");
    }
}

void process_requests(SharedMemory* shared_memory, HashTable<std::string, std::string>& hash_table) {
    is_running.store(true);
    while(is_running) {
        if (shared_memory->is_empty) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::lock_guard<std::mutex> lock(shared_memory_mutex);
        if (!shared_memory->is_empty) {
            auto &request = shared_memory->requests[shared_memory->read_index];
            switch (request.operation) {
                case Request::Operation::INSERT:
                    hash_table.insert(request.key, request.value);
                    break;
                case Request::Operation::READ:
                    hash_table.read_by_key(request.key);
                    break;
                case Request::Operation::DELETE:
                    hash_table.delete_by_key(request.key);
                    break;
            }

            shared_memory->read_index = (shared_memory->read_index + 1) % buffer_size;
            if (shared_memory->read_index == shared_memory->write_index) {
                shared_memory->is_empty = true;
            }

            shared_memory->is_full = false;
        }
    }
}


// Client
void enqueue_request(SharedMemory* shared_memory, const Request& request) {
    while (shared_memory->is_full) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::lock_guard<std::mutex> shared_memory_lock(shared_memory_mutex);
    shared_memory->requests[shared_memory->write_index] = request;
    shared_memory->write_index = (shared_memory->write_index + 1) % buffer_size;
    if ((shared_memory->write_index + 1) % buffer_size == shared_memory->read_index) {
        shared_memory->is_full = true;
    }
    shared_memory->is_empty = false;
}

void client_thread(SharedMemory* shared_memory, int id) {
    for (int i = 0; i < 5; i++) {
        std::string key = "key" + std::to_string(id * 10 + i);
        std::string value = "value" + std::to_string(id * 10 + i);

        enqueue_request(shared_memory, Request(Request::Operation::INSERT, key, value));
        enqueue_request(shared_memory, Request(Request::Operation::READ, key));
        enqueue_request(shared_memory, Request(Request::Operation::DELETE, key));
    }
}

#endif //SERVER_SHARED_MEMORY_H
