#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared_memory.h"

void enqueue_request(SharedMemory *shared_memory, const Request &request) {
    pthread_mutex_lock(&shared_memory->mutex);
    while ((shared_memory->write_index + 1) % BUFFER_SIZE == shared_memory->read_index) {
        pthread_cond_wait(&shared_memory->is_not_full, &shared_memory->mutex);
    }

    shared_memory->requests[shared_memory->write_index] = request;

    shared_memory->write_index = (shared_memory->write_index + 1) % BUFFER_SIZE;

    pthread_cond_signal(&shared_memory->is_not_empty);
    pthread_mutex_unlock(&shared_memory->mutex);
}

struct ClientThreadData {
    SharedMemory *shared_memory;
    int id;
};

void *client_thread(void *arg) {
    auto *data = (ClientThreadData *) arg;
    SharedMemory *shared_memory = data->shared_memory;
    int id = data->id;

    for (int i = 0; i < 5; i++) {
        std::string key = "key" + std::to_string(id * 10 + i);
        std::string value = "value" + std::to_string(id * 10 + i);

        enqueue_request(shared_memory, Request(Request::Operation::INSERT, key, value));
        enqueue_request(shared_memory, Request(Request::Operation::READ, key));
        enqueue_request(shared_memory, Request(Request::Operation::DELETE, key));
    }
    return nullptr;
}

int main() {
    int shared_memory_file_description = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shared_memory_file_description == -1) {
        std::perror("shm_open");
        return 1;
    }

    auto *shared_memory = (SharedMemory *) mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                shared_memory_file_description, 0);
    if (shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        return 1;
    }

    pthread_t thread1, thread2, thread3, thread4, thread5;

    ClientThreadData thread_data[] = {
            {shared_memory, 1},
            {shared_memory, 2},
            {shared_memory, 3},
            {shared_memory, 4},
            {shared_memory, 5},
    };

    if (pthread_create(&thread1, nullptr, client_thread, (void *) &thread_data[0]) != 0) {
        std::perror("pthread_create");
        return 1;
    }
    if (pthread_create(&thread2, nullptr, client_thread, (void *) &thread_data[0]) != 0) {
        std::perror("pthread_create");
        return 1;
    }
    if (pthread_create(&thread3, nullptr, client_thread, (void *) &thread_data[0]) != 0) {
        std::perror("pthread_create");
        return 1;
    }
    if (pthread_create(&thread4, nullptr, client_thread, (void *) &thread_data[0]) != 0) {
        std::perror("pthread_create");
        return 1;
    }
    if (pthread_create(&thread5, nullptr, client_thread, (void *) &thread_data[0]) != 0) {
        std::perror("pthread_create");
        return 1;
    }

    pthread_join(thread1, nullptr);
    pthread_join(thread2, nullptr);
    pthread_join(thread3, nullptr);
    pthread_join(thread4, nullptr);
    pthread_join(thread5, nullptr);

    munmap(shared_memory, SHM_SIZE);
    close(shared_memory_file_description);

    return 0;
}
