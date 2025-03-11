#include <new>
#include <stdio.h>

#include "SimpleAllocator.h"

SimpleAllocator::SimpleAllocator(): head{nullptr} {
	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		number_available[i] = 0;
	}
}

SimpleAllocator::~SimpleAllocator() {
	Chunk *former_head;

	while(head) {
		former_head = head;
		head = head->next;

		free(former_head);
	}
}

void *SimpleAllocator::allocate(int size) {
	int allocation_index = -1;

	for(auto i = 0; i < NUMBER_STANDARD_ALLOCATIONS; i++) {
		if(STANDARD_ALLOCATIONS[i] == size) {
			allocation_index = i;
			break;
		}
	}

	if(allocation_index != 1) {
		if(number_available[allocation_index] == 0) {
			char *chunk_block = (char *) malloc(sizeof(Chunk) + (CHUNK_SIZE * STANDARD_ALLOCATIONS[allocation_index]));

			head = new(chunk_block) Chunk((uint8_t) allocation_index, (chunk_block + sizeof(Chunk)), head);

			number_available[allocation_index] += CHUNK_SIZE;
		}

		for(Chunk *current = head; current != nullptr; current = current->next) {
			if(current->allocation_index != allocation_index) {
				continue;
			}

			for(auto i = 0; i < CHUNK_SIZE; i++) {
				if(!BITMAP_GET(current->bitmap, i)) {
					BITMAP_SET_ON(current->bitmap, i);

					number_available[allocation_index]--;
					current->number_available--;

					return reinterpret_cast<void *>(current->data + (i * STANDARD_ALLOCATIONS[current->allocation_index]));
				}
			}
		}
	}

	return malloc(size);
}

void SimpleAllocator::deallocate(void *pointer) {
	for(Chunk *current = head; current != nullptr; current = current->next) {
		if(pointer >= current->data && pointer < current->limit) {
			int position = (reinterpret_cast<char *>(pointer) - current->data) / STANDARD_ALLOCATIONS[current->allocation_index];

			BITMAP_SET_OFF(current->bitmap, position);

			number_available[current->allocation_index]++;
			current->number_available++;

			return;
		}
	}

	free(pointer);
}