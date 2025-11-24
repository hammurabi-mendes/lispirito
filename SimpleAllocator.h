#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

#include "types.h"
#include "LispNode.h"

class SimpleAllocator {
	static constexpr uint8_t NUMBER_STANDARD_ALLOCATIONS = 2;

	static constexpr uint8_t STANDARD_ALLOCATIONS[NUMBER_STANDARD_ALLOCATIONS] = {
		(sizeof(LispNode) + sizeof(CounterType)),
		(sizeof(Box) + sizeof(CounterType))
	};

	static constexpr uint8_t CHUNK_SIZE = 128;

	struct Chunk {
		char *next;
		char *start;
		char *limit;

		Chunk(uint8_t allocation_size);

		Chunk *next_chunk;
	};

	static Chunk *chunks[NUMBER_STANDARD_ALLOCATIONS];

public:
	static void init();
	static void finish();

	static void *allocate(int size);
	static void deallocate(void *pointer);
};

#endif /* SIMPLE_ALLOCATOR_H */