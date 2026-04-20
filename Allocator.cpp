#include "Allocator.hpp"

#include "LispNode.h"

static void call_destructor(void *pointer, uint8_t tag) {
    if(tag == 0) {
        static_cast<LispNode *>(pointer)->~LispNode();
    }
    else {
        static_cast<Box *>(pointer)->~Box();
    }
}

void *allocate_generic(CircularQueue &queue, size_t size, uint8_t tag) {
    void *recycled = queue.dequeue();

    if(recycled) {
        call_destructor(recycled, tag);
        return recycled;
    }

    CounterType *pointer = (CounterType *) Allocate(size + sizeof(CounterType));
    *pointer = 0;

    return pointer + 1;
}

void deallocate_generic(void *pointer) noexcept {
    Deallocate(((CounterType *) pointer) - 1);
}

bool process_deletions_generic(CircularQueue &queue, uint8_t tag) {
    bool deleted = false;

    while(!queue.is_empty_or_overflown()) {
        void *pointer = queue.dequeue();

        call_destructor(pointer, tag);
        deallocate_generic(pointer);

        deleted = true;
    }

    return deleted;
}

void reinit_generic(CircularQueue &queue, uint8_t tag) {
    void **old_queue = queue.reinit();

    for(size_t current = 0; current < CircularQueue::QUEUE_SIZE; current++) {
        call_destructor(old_queue[current], tag);
        deallocate_generic(old_queue[current]);

        while(process_deletions_generic(queue, tag)) {}
    }

    Deallocate(old_queue);
}

// Definitions of the static deletion queues

template<>
CircularQueue Allocator<LispNode>::deletion_queue{};

template<>
CircularQueue Allocator<Box>::deletion_queue{};
