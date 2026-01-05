#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <cstddef>
#include <cstdint>

class CircularQueue {
public:
    constexpr static size_t QUEUE_SIZE = UINT8_MAX + 1;

private:
    void **queue;

    uint8_t head;
    uint8_t tail;

public:
    CircularQueue();
    ~CircularQueue();

    void init();
    void** reinit();

    inline void enqueue(void *pointer) {
        queue[tail++] = pointer;
    }

    inline void* dequeue() {
        return (is_empty_or_overflown() ? nullptr : queue[head++]);
    }

    inline bool is_empty_or_overflown() const {
        return head == tail;
    }
};

#endif /* CIRCULAR_QUEUE_H */
