#ifndef SERVER_LINKED_LIST_H
#define SERVER_LINKED_LIST_H

#include <memory>
#include <iostream>
#include <optional>

template <typename K, typename T>
struct Node {
public:
    using key_type = K;
    using value_type = T;

    key_type key;
    value_type value;
    std::unique_ptr<Node<key_type , value_type>> next;

    Node(key_type key, value_type value) : key(key), value(value), next(nullptr) {}
};

template <typename K, typename T>
class LinkedList {
public:
    using key_type = K;
    using value_type = T;

    LinkedList() = default;

    void insert(key_type key, value_type value) {
        if (!_head) {
            _head = make_unique<Node<key_type, value_type>>(key, value);
            return;
        }

        Node<key_type, value_type>* temp = _head.get();
        while (temp) {
            if (temp->key == key) {
                temp->value = std::move(value);
                return;
            }

            if (temp->next) {
                temp = temp->next.get();
            } else {
                break;
            }
        }

        temp->next = std::make_unique<Node<key_type, value_type>>(key, value);
    }

    std::optional<value_type> read_by_key(const key_type& key) {
        Node<key_type, value_type>* temp = _head.get();
        while (temp) {
            if (temp->key == key) {
                return temp->value;
            }
            temp = temp->next.get();
        }

        std::cerr << "READ ERROR: " << key << " not found!" << std::endl;
        return std::nullopt;
    }

    void delete_by_key(const key_type& key) {
        if (!_head) {
            std::cerr << "DELETE ERROR: " << key << " not found!" << std::endl;
            return;
        }

        if (_head->key == key) {
            _head = std::move(_head->next);
            return;
        }

        Node<key_type, value_type>* temp = _head.get();
        while (temp && temp->next) {
            if (temp->next->key == key) {
                temp->next = std::move(temp->next->next);
                return;
            }
            temp = temp->next.get();
        }

        std::cerr << "DELETE ERROR 2: " << key << " not found!" << std::endl;
    }
private:
    std::unique_ptr<Node<key_type, value_type>> _head;
};

#endif //SERVER_LINKED_LIST_H
