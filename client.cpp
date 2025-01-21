#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared_memory.h"

void enqueue_request(SharedMemory *shared_memory, const Request &request, int id) {
    pthread_mutex_lock(&shared_memory->mutex);

    // Wait until buffer is not full
    while ((shared_memory->write_index + 1) % BUFFER_SIZE == shared_memory->read_index) {
        pthread_cond_wait(&shared_memory->is_not_full, &shared_memory->mutex);
    }

    // Register a new request
    switch (request.operation) {
        case Request::Operation::INSERT:
            std::printf("Client %d: Queuing INSERT operation (key: %s, value: %s)\n", id, request.key.c_str(), request.value.c_str());
            break;
        case Request::Operation::READ:
            std::printf("Client %d: Queuing READ operation (key: %s)\n", id, request.key.c_str());
            break;
        case Request::Operation::DELETE:
            std::printf("Client %d: Queuing DELETE operation (key: %s)\n", id, request.key.c_str());
            break;
    }
    shared_memory->requests[shared_memory->write_index] = request;

    // Move the write index for the circular buffer
    shared_memory->write_index = (shared_memory->write_index + 1) % BUFFER_SIZE;

    // Notify server threads that buffer is not empty
    pthread_cond_signal(&shared_memory->is_not_empty);
    pthread_mutex_unlock(&shared_memory->mutex);
}

struct ClientThreadData {
    SharedMemory *shared_memory;
    int id;
};

void *client_thread(void *arg) {
    auto data = static_cast<ClientThreadData *>(arg);
    SharedMemory *shared_memory = data->shared_memory;
    int id = data->id;

    for (int i = 0; i < 10; i++) {
        std::string key = std::to_string(id * 100 + i);
        std::string value = std::to_string(id * 100 + i);

        enqueue_request(shared_memory, Request(Request::Operation::INSERT, key, value), id);
        enqueue_request(shared_memory, Request(Request::Operation::READ, key), id);
        enqueue_request(shared_memory, Request(Request::Operation::DELETE, key), id);
    }

    return nullptr;
}

int main(int argc, char *argv[]) {
    // Validate command-line arguments
    if (argc != 2) {
        std::printf("You must provide exactly one argument: the number of threads!\n");
        return 1;
    }

    // Validate the number of threads argument
    std::string num_threads_arg = argv[1];
    for (auto c: num_threads_arg) {
        if (!std::isdigit(c)) {
            std::printf("Please provide a valid integer for the number of threads!\n");
            return 1;
        }
    }

    int num_threads = std::stoi(num_threads_arg);
    if (num_threads == 0) {
        std::printf("Please provide a positive integer for the number of threads!\n");
        return 1;
    }

    // Open shared memory
    int shared_memory_file_description = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shared_memory_file_description == -1) {
        std::perror("shm_open");
        return 1;
    }

    // Map the shared memory into the process' memory address
    auto *shared_memory = static_cast<SharedMemory *>(mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                shared_memory_file_description, 0));
    if (shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        return 1;
    }

    // Creating client threads
    pthread_t threads[num_threads];
    ClientThreadData thread_arguments[num_threads];
    for (int i = 0; i < num_threads; i++) {
        thread_arguments[i].shared_memory = shared_memory;
        thread_arguments[i].id = i;
        if (pthread_create(&threads[i], nullptr, client_thread, static_cast<void *>(&thread_arguments[i])) != 0) {
            std::perror("pthread_create");
            continue;
        }
    }

    // Waiting for all the client threads to finish
    for (auto& thread : threads) {
        if (pthread_join(thread, nullptr) != 0) {
            std::perror("Failed to join thread");
        }
    }

    // Clean up the shared memory
    munmap(shared_memory, SHM_SIZE);
    close(shared_memory_file_description);

    return 0;
}
