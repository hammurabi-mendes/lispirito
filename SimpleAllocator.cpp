#include <new>
#include <stdio.h>

#include "SimpleAllocator.h"

static constexpr uint8_t NUMBER_STANDARD_ALLOCATIONS = 2;

#ifdef REFERENCE_COUNTING
static constexpr uint8_t STANDARD_ALLOCATIONS[NUMBER_STANDARD_ALLOCATIONS] = {
	(sizeof(LispNode) + sizeof(CounterType)),
	(sizeof(Box) + sizeof(CounterType))
};
#else
static constexpr uint8_t STANDARD_ALLOCATIONS[NUMBER_STANDARD_ALLOCATIONS] = {
	sizeof(LispNode),
	sizeof(Box)
};
#endif /* REFERENCE_COUNTING */

inline void destruct_lispnode(void *pointer) {
	reinterpret_cast<LispNode *>(pointer)->~LispNode();
}

inline void destruct_box(void *pointer) {
	reinterpret_cast<Box *>(pointer)->~Box();
}

static constexpr uint8_t CHUNK_SIZE = 128;

void BITMAP_CLEAR(char *bitmap) {
	for(auto i = 0; i < CHUNK_SIZE / 8; i++) {
		bitmap[i] = 0;
	}
}

bool BITMAP_GET(char *bitmap, uint8_t offset) {
	return (bitmap[(offset) / 8] & (1U << ((offset) % 8)));
}

void BITMAP_SET_ON(char *bitmap, uint8_t offset) {
	bitmap[(offset) / 8] |= (1U << ((offset) % 8));
}

void BITMAP_SET_OFF(char *bitmap, uint8_t offset) {
	bitmap[(offset) / 8] &= ~(1U << ((offset) % 8));
}

struct Chunk {
	AllocationIndex allocation_index;
	unsigned int number_available;

	char bitmap[CHUNK_SIZE / 8];
	char bitmap2[CHUNK_SIZE / 8];
	char *data;
	char *limit;

	Chunk *next;

	Chunk(AllocationIndex allocation_index, char *data, Chunk *next);
};

Chunk::Chunk(AllocationIndex allocation_index, char *data, Chunk *next): allocation_index{allocation_index}, data{data}, next{next} {
	limit = data + (CHUNK_SIZE * STANDARD_ALLOCATIONS[allocation_index]);
	number_available = CHUNK_SIZE;

	BITMAP_CLEAR(bitmap);
}

static Chunk *head;
unsigned int number_available[NUMBER_STANDARD_ALLOCATIONS];

void SimpleAllocator::init() {
	head = nullptr;

	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		number_available[i] = 0;
	}
}

void SimpleAllocator::finish() {
	Chunk *previous_head;

	while(head) {
		previous_head = head;
		head = head->next;

		free(previous_head);
	}
}

void *SimpleAllocator::allocate(int size, AllocationIndex allocation_index) {
	if(allocation_index < AllocationIndex::IndexGeneric) {
		if(number_available[allocation_index] == 0) {
			char *chunk_block = (char *) malloc(sizeof(Chunk) + (CHUNK_SIZE * STANDARD_ALLOCATIONS[allocation_index]));

			head = new(chunk_block) Chunk(allocation_index, (chunk_block + sizeof(Chunk)), head);

			number_available[allocation_index] += CHUNK_SIZE;
		}

		for(Chunk *current = head; current != nullptr; current = current->next) {
			if(current->allocation_index != allocation_index) {
				continue;
			}

			for(auto i = 0; i < CHUNK_SIZE; i++) {
				if(!BITMAP_GET(current->bitmap, i)) {
					BITMAP_SET_ON(current->bitmap, i);

					number_available[current->allocation_index]--;
					current->number_available--;

					return reinterpret_cast<void *>(current->data + (i * STANDARD_ALLOCATIONS[current->allocation_index]));
				}
			}
		}
	}

	return malloc(size);
}

void do_deallocate(Chunk *chunk, int position) {
	BITMAP_SET_OFF(chunk->bitmap, position);

	number_available[chunk->allocation_index]++;
	chunk->number_available++;
}

void SimpleAllocator::deallocate(void *pointer) {
	for(Chunk *current = head; current != nullptr; current = current->next) {
		if(pointer >= current->data && pointer < current->limit) {
			int position = (reinterpret_cast<char *>(pointer) - current->data) / STANDARD_ALLOCATIONS[current->allocation_index];

			// The conditional is necessary because if we have reference counting
			// we must avoid dealocating an object twice when we start deallocating via mark-and-sweep
			if(BITMAP_GET(current->bitmap, position)) {
				do_deallocate(current, position);
			}

			return;
		}
	}

	free(pointer);
}

void SimpleAllocator::setup() {
	for(Chunk *current = head; current != nullptr; current = current->next) {
		BITMAP_CLEAR(current->bitmap2);
	}
}

void SimpleAllocator::set_mark(void *pointer) {
#ifdef REFERENCE_COUNTING
	pointer = reinterpret_cast<char *>(pointer) - sizeof(CounterType);
#endif /* REFERENCE_COUNTING */

	for(Chunk *current = head; current != nullptr; current = current->next) {
		if(pointer >= current->data && pointer < current->limit) {
			int position = (reinterpret_cast<char *>(pointer) - current->data) / STANDARD_ALLOCATIONS[current->allocation_index];

			BITMAP_SET_ON(current->bitmap2, position);
		}
	}
}

bool SimpleAllocator::get_mark(void *pointer) {
#ifdef REFERENCE_COUNTING
	pointer = reinterpret_cast<char *>(pointer) - sizeof(CounterType);
#endif /* REFERENCE_COUNTING */

	for(Chunk *current = head; current != nullptr; current = current->next) {
		if(pointer >= current->data && pointer < current->limit) {
			int position = (reinterpret_cast<char *>(pointer) - current->data) / STANDARD_ALLOCATIONS[current->allocation_index];

			return BITMAP_GET(current->bitmap2, position);
		}
	}

	return true;
}

void SimpleAllocator::commit() {
	for(Chunk *current = head; current != nullptr; current = current->next) {
		for(auto i = 0; i < CHUNK_SIZE; i++) {
			if(BITMAP_GET(current->bitmap, i) && !BITMAP_GET(current->bitmap2, i)) {
#ifdef REFERENCE_COUNTING
				void *object_location = current->data + (i * STANDARD_ALLOCATIONS[current->allocation_index]) + sizeof(CounterType);
#else
				void *object_location = current->data + (i * STANDARD_ALLOCATIONS[current->allocation_index]);
#endif /* REFERENCE_COUNTING */

				if(current->allocation_index == 0) {
					// ((LispNode *) object_location)->print();
					destruct_lispnode(object_location);
				}
				else {
					// ((Box *) object_location)->item->print();
					destruct_box(object_location);
				}

				do_deallocate(current, i);
			}
		}
	}
}