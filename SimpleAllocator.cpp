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

static constexpr uint8_t CHUNK_SIZE = 128;

struct Chunk {
	char *next;
	char *start;
	char *limit;
	uint8_t number_free;
	AllocationIndex allocation_index;

	char bitmap[CHUNK_SIZE / 8];
	char bitmap2[CHUNK_SIZE / 8];

	Chunk(AllocationIndex allocation_index);
	~Chunk();

	Chunk *next_chunk;
};

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

static Chunk *chunks[NUMBER_STANDARD_ALLOCATIONS];

int find_position(Chunk *chunk, void *pointer) {
	return (reinterpret_cast<char *>(pointer) - chunk->start) / STANDARD_ALLOCATIONS[chunk->allocation_index];
}

Chunk *find_chunk(void *pointer, int *position) {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			if(current->start <= pointer && pointer < current->limit) {
				if(position != nullptr) {
					*position = find_position(current, pointer);
				}

				return current;
			}
		}
	}

	return nullptr;
}

inline void destruct_lispnode(void *pointer) {
	reinterpret_cast<LispNode *>(pointer)->~LispNode();
}

inline void destruct_box(void *pointer) {
	reinterpret_cast<Box *>(pointer)->~Box();
}

Chunk::Chunk(AllocationIndex allocation_index): allocation_index{allocation_index} {
	int allocation_size = STANDARD_ALLOCATIONS[allocation_index];

	char *data = (char *) malloc(CHUNK_SIZE * allocation_size);

	char *position_curr = data;
	char *position_next = data + allocation_size;

	for(int i = 0; i < CHUNK_SIZE - 1; i++) {
		*((char **) position_curr) = position_next;

		position_curr = position_next;
		position_next += allocation_size;
	}

	*((char **) position_curr) = nullptr;

	this->start = data;
	this->limit = data + (CHUNK_SIZE * allocation_size);

	this->next = data;
	this->number_free = CHUNK_SIZE;

	this->next_chunk = nullptr;

	BITMAP_CLEAR(this->bitmap);
}

Chunk::~Chunk() {
	free(this->start);
}

void SimpleAllocator::init() {
	chunks[0] = new Chunk(AllocationIndex::IndexLispNode);
	chunks[1] = new Chunk(AllocationIndex::IndexBox);
}

void SimpleAllocator::finish() {
	Chunk *previous_chunk;

	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		while(chunks[i]) {
			previous_chunk = chunks[i];
			chunks[i] = chunks[i]->next_chunk;

			delete previous_chunk;
		}
	}
}

void *SimpleAllocator::allocate(int size, AllocationIndex allocation_index) {
	if(allocation_index < AllocationIndex::IndexGeneric) {
		Chunk *used_chunk = nullptr;

		// Try to find a chunk with free space
		for(Chunk *current = chunks[allocation_index]; current != nullptr; current = current->next_chunk) {
			if(current->next != nullptr) {
				used_chunk = current;
				break;
			}
		}

		// If a chunk is not found, allocate one and use it
		if(used_chunk == nullptr) {
			Chunk *new_chunk = new Chunk(allocation_index);

			new_chunk->next_chunk = chunks[allocation_index];
			chunks[allocation_index] = new_chunk;

			used_chunk = new_chunk;
		}

		// Allocate memory within the used chunk
		void *result = used_chunk->next;

		used_chunk->next = *((char **) result);
		used_chunk->number_free--;

		BITMAP_SET_ON(used_chunk->bitmap, find_position(used_chunk, result));
		return result;
	}

	return malloc(size);
}

void do_deallocate(Chunk *chunk, void *pointer, int position) {
	BITMAP_SET_OFF(chunk->bitmap, position);

	// Add the pointer back to the free list on the used chunk
	*((char **) pointer) = chunk->next;

	chunk->next = (char *) pointer;
	chunk->number_free++;
}

void SimpleAllocator::deallocate(void *pointer) {
	Chunk *chunk;
	int position;

	if((chunk = find_chunk(pointer, &position))) {
		if(BITMAP_GET(chunk->bitmap, position)) {
			do_deallocate(chunk, pointer, position);
		}

		return;
	}

	free(pointer);
}

void SimpleAllocator::setup() {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			BITMAP_CLEAR(current->bitmap2);
		}
	}
}

void SimpleAllocator::set_mark(void *pointer) {
	Chunk *chunk;
	int position;

#ifdef REFERENCE_COUNTING
	pointer = reinterpret_cast<char *>(pointer) - sizeof(CounterType);
#endif /* REFERENCE_COUNTING */

	if((chunk = find_chunk(pointer, &position))) {
		BITMAP_SET_ON(chunk->bitmap2, position);
	}
}

bool SimpleAllocator::get_mark(void *pointer) {
	Chunk *chunk;
	int position;

#ifdef REFERENCE_COUNTING
	pointer = reinterpret_cast<char *>(pointer) - sizeof(CounterType);
#endif /* REFERENCE_COUNTING */

	if((chunk = find_chunk(pointer, &position))) {
		return BITMAP_GET(chunk->bitmap2, position);
	}

	return true;
}

void SimpleAllocator::commit() {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			for(size_t position = 0; position < CHUNK_SIZE; position++) {
				if(!BITMAP_GET(current->bitmap, i) || BITMAP_GET(current->bitmap2, i)) {
					continue;
				}

				void *pointer = current->start + (position * STANDARD_ALLOCATIONS[current->allocation_index]);
#ifdef REFERENCE_COUNTING
				void *object_location = reinterpret_cast<char *>(pointer) + sizeof(CounterType);
#else
				void *object_location = pointer;
#endif /* REFERENCE_COUNTING */

				if(current->allocation_index == 0) {
					// ((LispNode *) object_location)->print();
					destruct_lispnode(object_location);
				}
				else {
					// ((Box *) object_location)->item->print();
					destruct_box(object_location);
				}

				do_deallocate(current, pointer, position);
			}
		}
	}
}