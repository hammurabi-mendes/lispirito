#ifndef C64_IO_H
#define C64_IO_H

#include <stdint.h>

struct C64_FILE;

void c64_fset(unsigned char device, unsigned char sa);

C64_FILE *c64_fopen(const char *name, const char *mode);
int c64_fclose(C64_FILE *descriptor);

char *c64_fgets(char *buffer, int size, C64_FILE *descriptor);
int c64_fputs(const char *buffer, C64_FILE *descriptor);

#endif // C64_IO_H