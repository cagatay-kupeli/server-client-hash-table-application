#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>

#include "shared_memory.h"
#include "hash_map.h"

SharedMemory *global_shared_memory = nullptr;  // handle_sigint utilizes this global variable

struct ServerThreadData {
    SharedMemory *shared_memory;
    HashTable<std::string, std::string> *hash_table;
};

void *server_thread(void *arg) {
    auto *data = (ServerThreadData *) arg;
    SharedMemory *shared_memory = data->shared_memory;
    HashTable<std::string, std::string> *hash_table = data->hash_table;
    shared_memory->is_running = true;

    while (shared_memory->is_running) {
        pthread_mutex_lock(&shared_memory->mutex);
        while (shared_memory->write_index == shared_memory->read_index && shared_memory->is_running) {
            pthread_cond_wait(&shared_memory->is_not_empty, &shared_memory->mutex);
        }

        if (!shared_memory->is_running) {
            break;
        }

        auto &request = shared_memory->requests[shared_memory->read_index];
        switch (request.operation) {
            case Request::Operation::INSERT:
                hash_table->insert(request.key, request.value);
                break;
            case Request::Operation::READ:
                hash_table->read_by_key(request.key);
                break;
            case Request::Operation::DELETE:
                hash_table->delete_by_key(request.key);
                break;
        }

        shared_memory->read_index = (shared_memory->read_index + 1) % BUFFER_SIZE;;

        pthread_cond_signal(&shared_memory->is_not_full);
        pthread_mutex_unlock(&shared_memory->mutex);
    }

    return nullptr;
}

void handle_sigint(int signal) {
    if (signal == SIGINT) {
        std::printf("\nSIGINT received. Stopping server...\n");
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
    if (argc != 2) {
        std::printf("You must provide exactly one argument, which is the hash table size!\n");
        return 1;
    }

    std::string arg = argv[1];
    for (auto c: arg) {
        if (!std::isdigit(c)) {
            std::printf("Please provide a valid integer for the hash table size!\n");
            return 1;
        }
    }

    std::size_t size = std::stoi(arg);
    HashTable<std::string, std::string> hash_table(size);

    int shared_memory_file_description = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shared_memory_file_description == -1) {
        std::perror("shm_open");
        return 1;
    }

    if (ftruncate(shared_memory_file_description, static_cast<off_t>(SHM_SIZE)) == -1) {
        std::perror("ftruncate");
        close(shared_memory_file_description);
        shm_unlink(SHM_NAME);
        return 1;
    }

    global_shared_memory = static_cast<SharedMemory *>(mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                           shared_memory_file_description, 0));
    if (global_shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        shm_unlink(SHM_NAME);
        return 1;
    }

    initialize_mutex(global_shared_memory);
    initialize_conditions(global_shared_memory);

    std::memset(global_shared_memory->requests, 0, BUFFER_SIZE);
    global_shared_memory->read_index = 0;
    global_shared_memory->write_index = 0;

    register_signal_action();

    pthread_t thread;
    ServerThreadData thread_data = {global_shared_memory, &hash_table};

    if (pthread_create(&thread, nullptr, server_thread, (void *) &thread_data) != 0) {
        std::perror("pthread_create");
        return 1;
    }

    pthread_join(thread, nullptr);

    clean_up_mutex(global_shared_memory);

    munmap(global_shared_memory, SHM_SIZE);
    close(shared_memory_file_description);
    shm_unlink(SHM_NAME);
    return 0;
}
