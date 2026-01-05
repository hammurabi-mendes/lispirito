#include "RCPointer.hpp"
#include "Allocator.hpp"

#include "LispNode.h"

#ifdef REFERENCE_COUNTING
template<typename T>
void RCPointer<T>::set(T *pointer_new) noexcept {
    if(pointer_new) {
        CounterType *reference_counter_new = ((CounterType *) pointer_new) - 1;
        (*reference_counter_new)++;
    }

    if(pointer) {
        CounterType *reference_counter = ((CounterType *) pointer) - 1;

        if(--(*reference_counter) == 0) {
            Allocator<T>::enqueue_for_deletion(pointer);
        }
    }

    pointer = pointer_new;
}

// Definition of the pointer setting functions
template void RCPointer<LispNode>::set(LispNode *pointer_new) noexcept;
template void RCPointer<Box>::set(Box *pointer_new) noexcept;

#endif /* REFERENCE_COUNTING */