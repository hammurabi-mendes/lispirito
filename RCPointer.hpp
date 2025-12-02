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
                delete pointer;
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

#endif /* RCPOINTER_HPP */
