#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <vector>

#include "shared_memory.h"
#include "hash_map.h"

// Global variable for shared memory, used in the SIGINT handler
SharedMemory *global_shared_memory = nullptr;

// Structure to hold data for each server thread
struct ServerThreadData {
    SharedMemory *shared_memory;
    HashTable<std::string, std::string> *hash_table;
    int id;
};

// Executed by each server thread
void *server_thread(void *arg) {
    auto *data = (ServerThreadData *) arg;
    SharedMemory *shared_memory = data->shared_memory;
    HashTable<std::string, std::string> *hash_table = data->hash_table;
    int id = data->id;

    while (shared_memory->is_running) {
        pthread_mutex_lock(&shared_memory->mutex);

        if (!shared_memory->is_running) {
            pthread_mutex_unlock(&shared_memory->mutex);
            std::printf("Server %d: Stopping the server...\n", id);
            break;
        }

        // Wait until buffer is not empty
        while (shared_memory->write_index == shared_memory->read_index && shared_memory->is_running) {
            pthread_cond_wait(&shared_memory->is_not_empty, &shared_memory->mutex);
        }

        if (!shared_memory->is_running) {
            pthread_mutex_unlock(&shared_memory->mutex);
            std::printf("Server %d: Stopping the server...\n", id);
            break;
        }

        // Process the current request
        auto &request = shared_memory->requests[shared_memory->read_index];
        switch (request.operation) {
            case Request::Operation::INSERT:
                std::printf("Server %d: Performing INSERT operation (key: %s, value: %s)\n", id, request.key.c_str(), request.value.c_str());
                hash_table->insert(request.key, request.value);
                break;
            case Request::Operation::READ:
                std::printf("Server %d: Performing READ operation (key: %s)\n", id, request.key.c_str());
                hash_table->read_by_key(request.key);
                break;
            case Request::Operation::DELETE:
                std::printf("Server %d: Performing DELETE operation (key: %s)\n", id, request.key.c_str());
                hash_table->delete_by_key(request.key);
                break;
        }

        // Move the read index for the circular buffer
        shared_memory->read_index = (shared_memory->read_index + 1) % BUFFER_SIZE;;

        // Notify client threads that buffer is not full
        pthread_cond_signal(&shared_memory->is_not_full);
        pthread_mutex_unlock(&shared_memory->mutex);
    }

    return nullptr;
}

// SIGINT handler to gracefully stop the server
void handle_sigint(int signal) {
    if (signal == SIGINT) {
        std::printf("\nSIGINT received!\n");
        global_shared_memory->is_running = false;

        pthread_mutex_lock(&global_shared_memory->mutex);
        pthread_cond_broadcast(&global_shared_memory->is_not_empty);
        pthread_mutex_unlock(&global_shared_memory->mutex);
    }
}

void register_signal_action() {
    struct sigaction sa{};
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}

int main(int argc, char *argv[]) {
    // Validate command-line arguments
    if (argc != 3) {
        std::printf("You must provide exactly two arguments: the hash table size and the number of threads!\n");
        return 1;
    }

    // Validate the hash table size argument
    std::string hash_table_size_arg = argv[1];
    for (auto c: hash_table_size_arg) {
        if (!std::isdigit(c)) {
            std::printf("Please provide a valid integer for the hash table size!\n");
            return 1;
        }
    }

    std::size_t hash_table_size = std::stoi(argv[1]);
    if (hash_table_size == 0) {
        std::printf("Please provide a positive integer for the hash table size!\n");
        return 1;
    }

    HashTable<std::string, std::string> hash_table(hash_table_size);

    // Validate the number of threads argument
    std::string num_threads_arg = argv[2];
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

    // Create and open shared memory
    int shared_memory_file_description = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shared_memory_file_description == -1) {
        std::perror("shm_open");
        return 1;
    }

    // Set the size of the shared memory object
    if (ftruncate(shared_memory_file_description, static_cast<off_t>(SHM_SIZE)) == -1) {
        std::perror("ftruncate");
        close(shared_memory_file_description);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // Map the shared memory into the process' memory address
    global_shared_memory = static_cast<SharedMemory *>(mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                           shared_memory_file_description, 0));
    if (global_shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // Initialize mutex and condition variables in the shared memory
    initialize_mutex(global_shared_memory);
    initialize_conditions(global_shared_memory);

    // Initialize the shared memory buffer
    std::memset(global_shared_memory->requests, 0, BUFFER_SIZE);
    global_shared_memory->read_index = 0;
    global_shared_memory->write_index = 0;
    global_shared_memory->is_running = true;

    // Register SIGINT signal handler
    register_signal_action();

    // Creating server threads
    pthread_t threads[num_threads];
    ServerThreadData thread_arguments[num_threads];
    for (int i = 0; i < num_threads; i++) {
        thread_arguments[i].shared_memory = global_shared_memory;
        thread_arguments[i].hash_table = &hash_table;
        thread_arguments[i].id = i;
        if (pthread_create(&threads[i], nullptr, server_thread, (void *) &thread_arguments[i]) != 0) {
            std::perror("pthread_create");
            continue;
        }
    }

    // Waiting for all the server threads to finish
    for (auto& thread : threads) {
        if (pthread_join(thread, nullptr) != 0) {
            std::perror("Failed to join thread");
        }
    }

    // Clean up the shared memory
    clean_up_mutex(global_shared_memory);

    munmap(global_shared_memory, SHM_SIZE);
    close(shared_memory_file_description);
    shm_unlink(SHM_NAME);
    return 0;
}
