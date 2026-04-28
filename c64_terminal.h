#ifndef TERMINAL_H
#define TERMINAL_H

extern bool join_lines;
extern char join_character;

unsigned int terminal_getline(char *buffer, unsigned int buffer_size, bool single_enter_capture = true);

#endif // TERMINAL_H