#ifndef EXTRA_H
#define EXTRA_H

#include "types.h"

constexpr unsigned int MAX_NUMERIC_STRING_LENGTH = 128;

void get_integral_string(Integral n, char *buffer);
void get_real_string(Real f, char *buffer);

void print_integral(Integral n);
void print_real(Real f);

#ifdef TARGET_6502
	#ifdef SIMPLE_ALLOCATOR
		#include "SimpleAllocator.h"

		extern SimpleAllocator allocator;

		#define Allocate allocator.allocate
		#define Deallocate allocator.deallocate
	#else
		#define Allocate malloc
		#define Deallocate free
	#endif /* SIMPLE_ALLOCATOR */

char *strdup(const char *input);
Real atof(char *input);
#endif /* TARGET_6502 */

#endif /* EXTRA_H */
