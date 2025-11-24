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
	this->number_free = CHUNK_SIZE;

	this->next_chunk = nullptr;
}

SimpleAllocator::Chunk::~Chunk() {
	free(this->start);
}

void SimpleAllocator::init() {
	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		chunks[i] = new Chunk(STANDARD_ALLOCATIONS[i]);
	}
}

void SimpleAllocator::finish() {
	compress(true);
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
			used_chunk->number_free--;

			return result;
		}
	}

	return malloc(size);
}

void SimpleAllocator::deallocate(void *pointer) {
	for(uint8_t i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		// Try to find a chunk that contained the free space
		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			if(current->start <= pointer && pointer < current->limit) {
				// Add the pointer back to the free list on the used chunk
				*((char **) pointer) = current->next;

				current->next = (char *) pointer;
				current->number_free++;

				return;
			}
		}
	}

	free(pointer);
}

void SimpleAllocator::compress(bool delete_all) {
	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		Chunk **last = &chunks[i];
		Chunk *next_delete = nullptr;

		for(Chunk *current = chunks[i]; current != nullptr; current = current->next_chunk) {
			if(next_delete) {
				delete next_delete;
				next_delete = nullptr;
			}

			if(delete_all || current->number_free == CHUNK_SIZE) {
				next_delete = current;

				*last = current->next_chunk;
			}
			else {
				last = &(current->next_chunk);
			}
		}

		if(next_delete != nullptr) {
			delete next_delete;
		}
	}
}