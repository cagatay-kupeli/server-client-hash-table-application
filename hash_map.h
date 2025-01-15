#ifndef SERVER_HASH_MAP_H
#define SERVER_HASH_MAP_H

#include <vector>

#include "linked_list.h"

template<typename Key, typename T>
class HashTable {
public:
    using key_type = Key;
    using value_type = T;

    explicit HashTable(std::size_t size) : _map(size), _locks(size) {}

    void insert(const key_type &key, const value_type &value) {
        std::size_t index = hash(key) % _map.size();
        std::lock_guard<std::mutex> lock(_locks[index]);
        _map[index].insert(key, value);
    }

    std::optional<value_type> read_by_key(const key_type &key) {
        std::size_t index = hash(key) % _map.size();
        std::lock_guard<std::mutex> lock(_locks[index]);
        return _map[index].read_by_key(key);
    }

    void delete_by_key(const key_type &key) {
        std::size_t index = hash(key) % _map.size();
        std::lock_guard<std::mutex> lock(_locks[index]);
        _map[index].delete_by_key(key);
    }

private:
    std::vector<LinkedList<key_type, value_type>> _map;
    std::vector<std::mutex> _locks;

    std::size_t hash(const key_type &key) {
        std::hash<key_type> hash_fn;
        return hash_fn(key);
    }
};

#endif //SERVER_HASH_MAP_H
