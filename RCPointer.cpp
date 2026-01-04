#include "RCPointer.hpp"

#include "LispNode.h"

template<>
LispNode **RCPointer<LispNode>::deletion_queue = nullptr;

template<>
uint8_t RCPointer<LispNode>::dq_head = 0;

template<>
uint8_t RCPointer<LispNode>::dq_tail = 0;

template<>
Box **RCPointer<Box>::deletion_queue = nullptr;

template<>
uint8_t RCPointer<Box>::dq_head = 0;

template<>
uint8_t RCPointer<Box>::dq_tail = 0;
