#ifndef EXTRA_H
#define EXTRA_H

#include "types.h"

#ifdef TARGET_6502
constexpr unsigned int MAX_NUMERIC_STRING_LENGTH = 32;
#else
constexpr unsigned int MAX_NUMERIC_STRING_LENGTH = 128;
#endif /* TARGET_6502 */

void get_integral_string(Integral n, char *buffer);
void get_real_string(Real f, char *buffer);

void print_integral(Integral n);
void print_real(Real f);

#define Allocate malloc
#define Deallocate free

#ifdef TARGET_6502
char *strdup(const char *input);
Real atof(char *input);
#endif /* TARGET_6502 */

#endif /* EXTRA_H */
