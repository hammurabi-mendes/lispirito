#ifndef EXTRA_H
#define EXTRA_H

#include <stdlib.h>

#include "types.h"

#ifdef TARGET_6502
constexpr unsigned int MAX_NUMERIC_STRING_LENGTH = 32;
#else
constexpr unsigned int MAX_NUMERIC_STRING_LENGTH = 128;
#endif /* TARGET_6502 */

void get_integral_string(Integral n, char *buffer);
void get_real_string(Real f, char *buffer);

void print_integral(Integral n, FILE *descriptor = stdout);
void print_real(Real f, FILE *descriptor = stdout);

#define Allocate malloc
#define Deallocate free

#ifdef TARGET_6502
char *strdup(const char *input);
Real atof(char *input);
#endif /* TARGET_6502 */

#ifdef TARGET_C64
	#include "c64_terminal.h"
	#include "c64_io.h"

	// Shadow standard names
	#define fopen(name, mode)               ((FILE *) c64_fopen(name, mode))
	#define fclose(descriptor)              c64_fclose(((C64_FILE *) descriptor))

	#define fgets(buffer, size, descriptor) c64_fgets(buffer, size, ((C64_FILE *) descriptor))
	#define fputs(buffer, descriptor)       c64_fputs(buffer, ((C64_FILE *) descriptor))

	#define IO_AVAILABLE 1
#endif // TARGET_C64

#endif /* EXTRA_H */
