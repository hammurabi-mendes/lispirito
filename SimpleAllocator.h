#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

#include "types.h"
#include "LispNode.h"

#define BITMAP_GET(bitmap, offset) (bitmap[offset / 8] & (1 << (offset % 8)))
#define BITMAP_SET_ON(bitmap, offset) (bitmap[offset / 8] |= (1 << (offset % 8)))
#define BITMAP_SET_OFF(bitmap, offset) (bitmap[offset / 8] &= ~(1 << (offset % 8)))

class SimpleAllocator {
	static constexpr uint16_t NUMBER_STANDARD_ALLOCATIONS = 2;

	static constexpr uint16_t STANDARD_ALLOCATIONS[NUMBER_STANDARD_ALLOCATIONS] = {
		(sizeof(LispNode) + sizeof(CounterType)),
		(sizeof(Box) + sizeof(CounterType))
	};

	static constexpr uint16_t CHUNK_SIZE = 64; // Make it multiple of 8

	// Allocator chunk for LispNode
	struct Chunk {
		uint8_t allocation_index : 1;
		uint8_t number_available: 7;

		char bitmap[CHUNK_SIZE / 8];
		char *data;
		char *limit;

		Chunk *next;

		Chunk(uint8_t allocation_index, char *data, Chunk *next): allocation_index{allocation_index}, data{data}, next{next} {
			limit = data + (CHUNK_SIZE * STANDARD_ALLOCATIONS[allocation_index]);
			number_available = CHUNK_SIZE;

			for(auto i = 0; i < CHUNK_SIZE / 8; i++) {
				bitmap[i] = 0;
			}
		}
	};

	Chunk *head;

	uint16_t number_available[NUMBER_STANDARD_ALLOCATIONS];

public:
	SimpleAllocator();
	~SimpleAllocator();

	void *allocate(int size);
	void deallocate(void *pointer);
};

#endif /* SIMPLE_ALLOCATOR_H */