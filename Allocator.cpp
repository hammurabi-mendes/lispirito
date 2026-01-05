#include "Allocator.hpp"

#include "LispNode.h"

// Definitions of the static deletion queues

template<>
CircularQueue Allocator<LispNode>::deletion_queue{};

template<>
CircularQueue Allocator<Box>::deletion_queue{};
