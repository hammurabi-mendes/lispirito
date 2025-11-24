#include <new>
#include <stdio.h>

#include "SimpleAllocator.h"

SimpleAllocator::Chunk *SimpleAllocator::chunks[SimpleAllocator::NUMBER_STANDARD_ALLOCATIONS];

SimpleAllocator::Chunk::Chunk(uint8_t allocation_size) {
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
	this->next_chunk = nullptr;
}

void SimpleAllocator::init() {
	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		chunks[i] = new Chunk(STANDARD_ALLOCATIONS[i]);
	}
}

void SimpleAllocator::finish() {
	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		Chunk *former_head;

		while(chunks[i]) {
			former_head = chunks[i];
			chunks[i] = chunks[i]->next_chunk;

			free(former_head);
		}
	}
}

void *SimpleAllocator::allocate(int size) {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		if(STANDARD_ALLOCATIONS[i] == size) {
			Chunk *used_chunk = nullptr;

			// Try to find a chunk with free space
			for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
				if(current->next != nullptr) {
					used_chunk = current;
					break;
				}
			}

			// If a chunk is not found, allocate one and use it
			if(used_chunk == nullptr) {
				Chunk *new_chunk = new Chunk(STANDARD_ALLOCATIONS[i]);

				new_chunk->next_chunk = chunks[i];
				chunks[i] = new_chunk;

				used_chunk = new_chunk;
			}

			// Allocate memory within the used chunk
			void *result = used_chunk->next;
			used_chunk->next = *((char **) result);

			return result;
		}
	}

	return malloc(size);
}

void SimpleAllocator::deallocate(void *pointer) {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			if(current->start <= pointer && pointer < current->limit) {
				// Add the pointer back to the free list on the used chunk
				*((char **) pointer) = current->next;
				current->next = (char *) pointer;

				return;
			}
		}
	}

	free(pointer);
}