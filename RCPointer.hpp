#ifndef RCPOINTER_HPP
#define RCPOINTER_HPP

#include <cstddef>

#include "types.h"

template<typename T>
class RCPointer {
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

    ~RCPointer() {
        set(nullptr);
    }

    T &operator*() const { return *pointer; }
    T *operator->() const { return pointer; }

    T *get_pointer() const {
        return pointer;
    }

private:
    void set(T *pointer_new) noexcept {
        if(pointer_new) {
            CounterType *reference_counter_new = ((CounterType *) pointer_new) - 1;

            (*reference_counter_new)++;
        }

        if(pointer) {
            CounterType *reference_counter = ((CounterType *) pointer) - 1;

            if(--(*reference_counter) == 0) {
                delete pointer;
            }
        }

        pointer = pointer_new;
    }
};

#endif /* RCPOINTER_HPP */
