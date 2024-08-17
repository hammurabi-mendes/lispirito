#ifndef RCPOINTER_HPP
#define RCPOINTER_HPP

#include <cstddef>

#ifdef TARGET_6502
using CounterType = unsigned int;
#else
using CounterType = unsigned long;
#endif /* TARGET_6502 */

template<typename T>
class RCPointer {
private:
    T* pointer;
    CounterType *reference_count;

public:
    __attribute__((noinline)) RCPointer(T *pointer = nullptr): pointer(nullptr), reference_count(nullptr) {
        if(pointer != nullptr) {
            this->pointer = pointer;
            this->reference_count = new CounterType(1);
        }
    }

    __attribute__((noinline)) RCPointer(const RCPointer &other) {
        set(other.pointer, other.reference_count);
    }

    __attribute__((noinline)) RCPointer &operator=(T *other_pointer) {
        if(this->pointer != other_pointer) {
            clear();
            set(other_pointer, new CounterType(1));
        }

        return *this;
    }

    __attribute__((noinline)) RCPointer &operator=(const RCPointer &other) {
        if(this != &other) {
            clear();
            set(other.pointer, other.reference_count);
        }

        return *this;
    }

    __attribute__((noinline)) RCPointer(RCPointer &&other) noexcept {
        set(other.pointer, other.reference_count);

        other.pointer = nullptr;
        other.reference_count = nullptr;
    }

    __attribute__((noinline)) RCPointer &operator=(RCPointer &&other) noexcept {
        if(this != &other) {
            clear();
            set(other.pointer, other.reference_count);

            other.pointer = nullptr;
            other.reference_count = nullptr;
        }

        return *this;
    }

    bool operator==(const RCPointer &other) const {
        return (*pointer == *(other.pointer));
    }

    bool operator==(const T *other_pointer) const {
        return (*pointer == *other_pointer);
    }

    bool operator==(std::nullptr_t) const {
        return (pointer == nullptr);
    }

    bool operator!=(std::nullptr_t) const {
        return (pointer != nullptr);
    }

    __attribute__((noinline)) RCPointer& operator=(std::nullptr_t) {
        clear();
        return *this;
    }

    ~RCPointer() {
        clear();
    }

    T &operator*() const { return *pointer; }
    T *operator->() const { return pointer; }

    T *get_pointer() const {
        return pointer;
    }

private:
    __attribute__((noinline)) void set(T *pointer_new, CounterType *reference_count_new) noexcept {
        pointer = pointer_new;

        if(pointer_new != nullptr) {
            reference_count = reference_count_new;
            (*reference_count)++;
        }
    }

    __attribute__((noinline)) void clear() noexcept {
        if(pointer != nullptr && --(*reference_count) == 0) {
            delete pointer;
            delete reference_count;

            pointer = nullptr;
            reference_count = nullptr;
        }
    }
};

#endif /* RCPOINTER_HPP */