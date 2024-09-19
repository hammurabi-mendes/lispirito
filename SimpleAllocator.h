#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

class SimpleAllocator {
	constexpr static int BITMAP_BYTES = 7;
	constexpr static int DATA_BYTES = 120;

#define BITMAP_GET(bitmap, offset) (bitmap[offset / BITMAP_BYTES] & (1 << (offset % 8)))
#define BITMAP_SET_ON(bitmap, offset) (bitmap[offset / BITMAP_BYTES] |= (1 << (offset % 8)))
#define BITMAP_SET_OFF(bitmap, offset) (bitmap[offset / BITMAP_BYTES] &= ~(1 << (offset % 8)))

#define DATA_GET(data, size, i) (reinterpret_cast<char *>(data) + (i * size))

	struct Chunk {
		unsigned int size : 6;
		unsigned int reserved: 2;
		char bitmap[BITMAP_BYTES];
		char data[DATA_BYTES];
	};

	void *base;
	void *limit;

	unsigned int bytes_available;

public:
	SimpleAllocator(): base{nullptr}, limit{nullptr}, bytes_available{0} {}

	void init(void *base, void *limit);

	void *malloc(int size);
	void free(void *pointer);

	int available() { return bytes_available; }
};

#endif /* SIMPLE_ALLOCATOR_H */