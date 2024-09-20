#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

class SimpleAllocator {
	constexpr static int DATA_BYTES = 127;

#define BITMAP_GET(bitmap, offset) (bitmap[offset / 8] & (1 << (offset % 8)))
#define BITMAP_SET_ON(bitmap, offset) (bitmap[offset / 8] |= (1 << (offset % 8)))
#define BITMAP_SET_OFF(bitmap, offset) (bitmap[offset / 8] &= ~(1 << (offset % 8)))

#define DATA_GET(data, size, i) (static_cast<char *>(data) + (i * size))

	struct Chunk {
		unsigned int size : 6;
		unsigned int reserved: 2;
		char bitmap[DATA_BYTES];

		unsigned int bitmap_bytes() {
			if(size >= 16) {
				return 1;
			}
			if(size >= 8) {
				return 2;
			}
			if(size >= 4) {
				return 4;
			}
			if(size >= 2) {
				return 8;
			}
			return 16;
		}

		unsigned int data_bytes() {
			return DATA_BYTES - bitmap_bytes();
		}

		char *data() {
			return bitmap + bitmap_bytes();
		}
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