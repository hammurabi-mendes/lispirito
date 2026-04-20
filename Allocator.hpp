#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "types.h"
#include "extra.h"

#include "circular_queue.h"

// Type-erased backing functions for all allocator instances

void *allocate_generic(CircularQueue &queue, size_t size, uint8_t tag);
void deallocate_generic(void *pointer) noexcept;
bool process_deletions_generic(CircularQueue &queue, uint8_t tag);
void reinit_generic(CircularQueue &queue, uint8_t tag);

template<typename T>
class Allocator {
private:
    static CircularQueue deletion_queue;
    static constexpr uint8_t TAG = UINT8_MAX;

public:
    static void init() {
        deletion_queue.init();
    }

    static void *allocate(size_t size) {
        return allocate_generic(deletion_queue, size, TAG);
    }

    static void deallocate(void *pointer) noexcept {
        deallocate_generic(pointer);
    }

    static void enqueue_for_deletion(T *pointer) {
        deletion_queue.enqueue(pointer);

        // If the queue overflows it looks empty
        //
        // This should happen only sporadically, and the reinit() function
        // takes care that the deletion queue does not get overflown again,
        // using iteration instead of recursion
        if(deletion_queue.is_empty_or_overflown()) {
            reinit_generic(deletion_queue, TAG);
        }
    }

    static bool process_deletions() {
        return process_deletions_generic(deletion_queue, TAG);
    }
};

// Declarations of the static deletion queues
struct LispNode;
struct Box;

template<>
CircularQueue Allocator<LispNode>::deletion_queue;

template<>
constexpr uint8_t Allocator<LispNode>::TAG = 0;

template<>
CircularQueue Allocator<Box>::deletion_queue;

template<>
constexpr uint8_t Allocator<Box>::TAG = 1;

#endif /* ALLOCATOR_HPP */
