#include "SimpleAllocator.h"

void SimpleAllocator::init(void *base, void *limit) {
	this->base = base;
	this->limit = limit;

	for(Chunk *chunk = reinterpret_cast<Chunk *>(base); chunk < reinterpret_cast<Chunk *>(limit); chunk += 1) {
		chunk->size = 0;

		this->bytes_available += DATA_BYTES;
	}
}

void *SimpleAllocator::malloc(int size) {
	Chunk *free_chunk = nullptr;

	for(Chunk *chunk = reinterpret_cast<Chunk *>(base); chunk < reinterpret_cast<Chunk *>(limit); chunk += 1) {
		// Ignore the free chunks for now, but keep track of the first one found
		if(chunk->size == 0) {
			if(free_chunk == nullptr) {
				free_chunk = chunk;
			}

			continue;
		}

		// If we found a non-free chunk with block size that's different from our required size, try a different one
		if(chunk->size != size) {
			continue;
		}

		// If we find an appropriately-sized, non-free chunk with an empty slot, use the slot
		for(int i = 0; i < (chunk->data_bytes() / chunk->size); i++) {
			if(BITMAP_GET(chunk->bitmap, i) == 0) {
				BITMAP_SET_ON(chunk->bitmap, i);

				// Adjust bytes_available for the allocation
				bytes_available -= chunk->size;

				return DATA_GET(chunk->data(), chunk->size, i);
			}
		}
	}

	// If we could not allocate in the non-free chunks above, create a new one
	if(free_chunk) {
		free_chunk->size = size;

		// Adjust bytes_available for internal fragmentation loss and the actual allocation
		bytes_available -= (free_chunk->data_bytes() % free_chunk->size);
		bytes_available -= free_chunk->size;

		for(int i = 0; i < free_chunk->bitmap_bytes(); i++) {
			free_chunk->bitmap[i] = 0;
		}

		BITMAP_SET_ON(free_chunk->bitmap, 0);

		return DATA_GET(free_chunk->data(), free_chunk->size, 0);
	}

	return nullptr;
}

void SimpleAllocator::free(void *pointer) {
	unsigned int chunk_number = (reinterpret_cast<char *>(pointer) - reinterpret_cast<char *>(base)) / sizeof(Chunk);

	Chunk *chunk = reinterpret_cast<Chunk *>(reinterpret_cast<Chunk *>(base) + chunk_number);
	
	unsigned int chunk_position = (reinterpret_cast<char *>(pointer) - chunk->data()) / chunk->size;

	bytes_available += chunk->size;
	BITMAP_SET_OFF(chunk->bitmap, chunk_position);
}