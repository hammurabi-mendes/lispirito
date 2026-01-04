#ifndef RCPOINTER_HPP
#define RCPOINTER_HPP

#include <cstddef>
#include <cstdint>

#include "types.h"

template<typename T>
class RCPointer {
    //
    // Definitions related to deferred deletion
    //

private:
    constexpr static size_t DELETION_QUEUE_SIZE = UINT8_MAX + 1;

    static T **deletion_queue;
    static uint8_t dq_head;
    static uint8_t dq_tail;

public:
    static void init() {
        deletion_queue = new T*[DELETION_QUEUE_SIZE];
        dq_head = 0;
        dq_tail = 0;
    }

    static size_t process_deletions() {
        size_t counter = 0;

        while(dq_head != dq_tail) {
            delete deletion_queue[dq_head++];
            counter++;
        }

        return counter;
    }

private:
    static void reinit() {
        T **old_deletion_queue = deletion_queue;

        init();

        for(size_t current = 0; current < DELETION_QUEUE_SIZE; current++) {
            delete old_deletion_queue[current];

            while(process_deletions() > 0) {
                // Keep cleaning...
            }
        }

        delete[] old_deletion_queue;
    }

    //
    // Definitions related to proper reference counting
    //

private:
    T *pointer;

public:
    RCPointer(): pointer(nullptr) {
    }

    RCPointer(T *pointer): pointer(nullptr) {
        set(pointer);
    }

    RCPointer(const RCPointer &other): pointer{nullptr} {
        set(other.pointer);
    }

    RCPointer(RCPointer &&other) noexcept: pointer{other.pointer} {
        other.pointer = nullptr;
    }

    RCPointer &operator=(T *other_pointer) {
        if(pointer != other_pointer) {
            set(other_pointer);
        }

        return *this;
    }

    bool operator==(const T *other_pointer) const {
        return (*pointer == *other_pointer);
    }

    RCPointer &operator=(const RCPointer &other) {
        if(this != &other) {
            set(other.pointer);
        }

        return *this;
    }

    RCPointer &operator=(RCPointer &&other) noexcept {
        if(this != &other) {
            set(nullptr);

            pointer = other.pointer;
            other.pointer = nullptr;
        }

        return *this;
    }

    bool operator==(const RCPointer &other) const {
        return (*pointer == *(other.pointer));
    }

    RCPointer& operator=(std::nullptr_t) {
        set(nullptr);

        return *this;
    }

    bool operator==(std::nullptr_t) const {
        return (pointer == nullptr);
    }

    bool operator!=(std::nullptr_t) const {
        return (pointer != nullptr);
    }

    ~RCPointer() {
#ifdef REFERENCE_COUNTING
        set(nullptr);
#endif /* REFERENCE_COUNTING */
    }

    T &operator*() const { return *pointer; }
    T *operator->() const { return pointer; }

    T *get_pointer() const {
        return pointer;
    }

private:
#ifdef REFERENCE_COUNTING
    void set(T *pointer_new) noexcept {
        if(pointer_new) {
            CounterType *reference_counter_new = ((CounterType *) pointer_new) - 1;

            (*reference_counter_new)++;
        }

        if(pointer) {
            CounterType *reference_counter = ((CounterType *) pointer) - 1;

            if(--(*reference_counter) == 0) {
                deletion_queue[dq_tail++] = pointer;

                // This should happen only sporadically, and the reinit() function
                // takes care that the deletion queue does not get overflown again,
                // using iteration instead of recursion
                if(dq_tail == dq_head) {
                    reinit();
                }
            }
        }

        pointer = pointer_new;
    }
#else
    inline void set(T *pointer_new) noexcept {
        pointer = pointer_new;
    }
#endif /* REFERENCE_COUNTING */
};

// Explicit template specialization declarations

struct LispNode;
struct Box;

template<>
LispNode **RCPointer<LispNode>::deletion_queue;

template<>
uint8_t RCPointer<LispNode>::dq_head;

template<>
uint8_t RCPointer<LispNode>::dq_tail;

template<>
Box **RCPointer<Box>::deletion_queue;

template<>
uint8_t RCPointer<Box>::dq_head;

template<>
uint8_t RCPointer<Box>::dq_tail;

#endif /* RCPOINTER_HPP */
