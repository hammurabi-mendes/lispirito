#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

#include "types.h"
#include "LispNode.h"

enum AllocationIndex : uint8_t {
	IndexLispNode = 0,
	IndexBox = 1,
	IndexGeneric = 2
};

struct SimpleAllocator {
	static void init();
	static void finish();

	static void *allocate(int size, AllocationIndex allocation_index = IndexGeneric);
	static void deallocate(void *pointer);

	// Mark-and-sweep GC functions

	static void setup();
	static void set_mark(void *pointer);
	static bool get_mark(void *pointer);
	static void commit();
};

#endif /* SIMPLE_ALLOCATOR_H */