#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "types.h"
#include "extra.h"

#include "circular_queue.h"

template<typename T>
class Allocator {
private:
    static CircularQueue deletion_queue;

public:
    static void init() {
        deletion_queue.init();
    }

    static void *allocate(size_t size) {
        T *recycled;

        if((recycled = static_cast<T *>(deletion_queue.dequeue())) != nullptr) {
            recycled->~T();

            return recycled;
        }

        CounterType *pointer = (CounterType *) Allocate(size + sizeof(CounterType));
        *pointer = 0;

        return pointer + 1;
    }

    static void deallocate(void *pointer) noexcept {
        Deallocate(((CounterType *) pointer) - 1);
    }

    static void enqueue_for_deletion(T *pointer) {
        deletion_queue.enqueue(pointer);

        // If the queue overflows it looks empty
        //
        // This should happen only sporadically, and the reinit() function
        // takes care that the deletion queue does not get overflown again,
        // using iteration instead of recursion
        if(deletion_queue.is_empty_or_overflown()) {
            reinit();
        }
    }

    static bool process_deletions() {
        bool deleted = false;

        while(!deletion_queue.is_empty_or_overflown()) {
            T* pointer = static_cast<T*>(deletion_queue.dequeue());

            delete pointer;
            deleted = true;
        }

        return deleted;
    }

private:
    static void reinit() {
        T **old_deletion_queue = reinterpret_cast<T**>(deletion_queue.reinit());

        for(size_t current = 0; current < CircularQueue::QUEUE_SIZE; current++) {
            delete old_deletion_queue[current];

            while(process_deletions() == true) {
                // Keep cleaning...
            }
        }

        delete[] old_deletion_queue;
    }
};

// Declarations of the static deletion queues
struct LispNode;
struct Box;

template<>
CircularQueue Allocator<LispNode>::deletion_queue;

template<>
CircularQueue Allocator<Box>::deletion_queue;

#endif /* ALLOCATOR_HPP */
