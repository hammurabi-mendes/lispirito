#include "circular_queue.h"

CircularQueue::CircularQueue(): queue{nullptr}, head{0}, tail{0} {
}

CircularQueue::~CircularQueue() {
    if(queue != nullptr) {
        delete[] queue;
    }
}

void CircularQueue::init() {
    queue = new void*[QUEUE_SIZE];

    head = 0;
    tail = 0;
}

void** CircularQueue::reinit() {
    void **old_queue = queue;

    init();

    return old_queue;
}