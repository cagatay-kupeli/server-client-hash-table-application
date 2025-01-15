#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared_memory.h"

int main() {
    int shared_memory_file_description = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shared_memory_file_description == -1) {
        std::perror("shm_open");
        return 1;
    }

    auto* shared_memory = (SharedMemory*) mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_file_description, 0);
    if (shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        return 1;
    }

    std::thread thread1(client_thread, shared_memory, 1);
    std::thread thread2(client_thread, shared_memory, 2);
    std::thread thread3(client_thread, shared_memory, 3);
    std::thread thread4(client_thread, shared_memory, 4);
    std::thread thread5(client_thread, shared_memory, 5);

    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();

    munmap(shared_memory, SHM_SIZE);
    close(shared_memory_file_description);

    return 0;
}
