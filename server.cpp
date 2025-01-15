#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <csignal>

#include "shared_memory.h"
#include "hash_map.h"

int main(int argc, char* argv[]) {
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

    std::signal(SIGINT, sigint_handler);

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

    auto* shared_memory = (struct SharedMemory*) mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_file_description, 0);
    if (shared_memory == MAP_FAILED) {
        std::perror("mmap");
        close(shared_memory_file_description);
        return 1;
    }

    shared_memory->is_empty = true;
    shared_memory->is_full = false;

    std::thread thread(process_requests, shared_memory, std::ref(hash_table));

    thread.join();

    munmap(shared_memory, SHM_SIZE);
    close(shared_memory_file_description);
    shm_unlink(SHM_NAME);
    return 0;
}
