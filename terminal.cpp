#include <cbm.h>
#include <stdio.h>
#include <stdbool.h>

// External knobs
bool join_lines = false;
char join_character = '\n';

#define SCREEN ((unsigned char *) 0x0400)
#define SCREEN_WIDTH 40
#define SCREEN_HEIGHT 25

#define CH_CLR 147
#define CURSOR_CHAR 0xA0

static unsigned char base_column;
static unsigned char base_row;

static unsigned char current_column;
static unsigned char current_row;

static unsigned char limit_column;
static unsigned char limit_row;

static unsigned char char_under_cursor = 32;

inline unsigned char *get_line_pointer() {
    // Use PNT ($D1/$D2) - pointer to current screen line (handles linked lines correctly)
    return ((unsigned char *) (*((unsigned char *) 209) | (*((unsigned char *) 210) << 8)));
}

inline void restore_original_character(void) {
    unsigned char col = *(unsigned char*) 211;

    unsigned char *line = get_line_pointer();
    line[col] = char_under_cursor;
}

inline void show_cursor(void) {
    unsigned char col = *(unsigned char*) 211;

    unsigned char *line = get_line_pointer();

    char_under_cursor = line[col];
    line[col] = CURSOR_CHAR;
}

inline void get_cursor(unsigned char *column, unsigned char *row) {
    *column = *(unsigned char *) 211;
    *row    = *(unsigned char *) 214;
}

inline void set_cursor(unsigned char column, unsigned char row) {
    *(unsigned char *) 211 = column;
    *(unsigned char *) 214 = row;
}


static bool screen_line_is_empty(unsigned int row, unsigned int start, unsigned int end) {
    unsigned char *line = get_line_pointer();

    for(unsigned int column = start; column < end; column++) {
        if(line[column] != 32) {
            return false;
        }
    }

    return true;
}

static unsigned char screencode_to_ascii(unsigned char sc) {
    if(sc >= 1 && sc <= 26) {
        return sc + 96;
    }

    if(sc >= 65 && sc <= 90) {
        return sc;
    }

    return sc;
}

static unsigned int copy_screen_line(unsigned int row, char *destination, unsigned int size, unsigned int start = 0, unsigned int finish = SCREEN_WIDTH) {
    unsigned int end = start;

    for(unsigned int column = start; column < finish; column++) {
        if(SCREEN[row * SCREEN_WIDTH + column] != 32) {
            end = column + 1;
        }
    }

    unsigned int length = 0;

    for(unsigned int column = start; column < end && length < size; column++) {
        destination[length++] = screencode_to_ascii(SCREEN[row * SCREEN_WIDTH + column]);
    }

    if(join_lines) {
        if(length > 0 && destination[length - 1] != join_character) {
            destination[length++] = join_character;
        }
    }

    return length;
}

static bool scroll() {
    if(current_row == SCREEN_HEIGHT - 1) {
        if(base_row == 0) {
            // Maximum scrolling limit reached
            return false;
        }

        base_row--;
    }

    cbm_k_chrout(CH_ENTER);
    return true;
}

bool within_bounds(int8_t offset_col, int8_t offset_row) {
    int8_t current_column_off = current_column + offset_col;
    int8_t current_row_off = current_row + offset_row;

    if(current_row_off < 0 || current_column_off < 0 || current_row_off >= SCREEN_HEIGHT || current_column_off >= SCREEN_WIDTH) {
        return false;
    }

    if(base_row > 0 && current_row_off < base_row) {
        return false;
    }

    if(limit_row < SCREEN_HEIGHT - 1 && current_row_off > limit_row) {
        return false;
    }

    if(current_row_off == base_row && base_column > 0 && current_column_off < base_column) {
        return false;
    }

    if(current_row_off == limit_row && limit_column < SCREEN_WIDTH - 1 && current_column_off > limit_column) {
        return false;
    }

    return true;
}

unsigned int terminal_getline(char *buffer, unsigned int buffer_size, bool single_enter_capture = true) {
    // Lisp-specific
    int total_open = 0;
	int total_close = 0;

    unsigned int buffer_used = 0;

    get_cursor(&base_column, &base_row);

    current_column = limit_column = base_column;
    current_row = limit_row = base_row;

    unsigned char character;

    while(buffer_used < buffer_size) {
        show_cursor();

        while((character = cbm_k_getin()) == 0) {
            // Spin!
        }

        get_cursor(&current_column, &current_row);
        restore_original_character();

        // Update the limit positions
        if(!within_bounds(0, 0)) {
            limit_column = current_column;
            limit_row = current_row;
        }

        if(character == CH_FONT_LOWER || character == CH_FONT_UPPER) {
            continue;
        }

        if(character == CH_HOME) {
            set_cursor(base_column, base_row);
            continue;
        }

        if(character == CH_CLR) {
            continue;
        }

        if(character == CH_CURS_UP || character == CH_CURS_DOWN || character == CH_CURS_LEFT || character == CH_CURS_RIGHT || character == CH_DEL) {
            uint8_t offset_row = 0;
            uint8_t offset_col = 0;

            switch(character) {
                case CH_CURS_UP:
                    offset_row--;
                    break;
                case CH_CURS_DOWN:
                    offset_row++;
                    break;
                case CH_CURS_LEFT:
                    offset_col--;
                    break;
                case CH_CURS_RIGHT:
                    offset_col++;
                    break;
                case CH_DEL:
                    offset_col--;
                    break;
            }

            if(within_bounds(offset_col, offset_row)) {
                cbm_k_chrout(character);
            }

            continue;
        }

        if(character == CH_ENTER) {
            // If we are about to issue ENTER in an empty line, we will exit right after
            bool should_break = false;

            unsigned char line_start = (current_row == base_row) ? base_column : 0;

            if(single_enter_capture || screen_line_is_empty(current_row, line_start, SCREEN_WIDTH)) {
                should_break = true;
            }

            if(total_open <= total_close) {
                should_break = true;
            }

            // If we should break or run out of screen space, return all characters
            if(scroll() == false || should_break) {
                break;
            }
        }
        else {
            if(character == '(') {
				total_open++;
			}

			if(character == ')') {
				total_close++;
			}

            cbm_k_chrout(character);
        }
    }

    // Display the cursor again and collect data
    show_cursor();

    // Get curent position
    unsigned char end_column;
    unsigned char end_row;

    get_cursor(&end_column, &end_row);

    // Perform the copy
    unsigned int buffer_position = 0;

	buffer_position += copy_screen_line(base_row, buffer + buffer_position, buffer_size - buffer_position, base_column, SCREEN_WIDTH);

    for(unsigned int row = base_row + 1; row <= end_row - 1; row++) {
        buffer_position += copy_screen_line(row, buffer + buffer_position, buffer_size - buffer_position, 0U, SCREEN_WIDTH);
    }

	buffer_position += copy_screen_line(end_row, buffer + buffer_position, buffer_size - buffer_position, 0U, end_column);

    // Zeroes last character if we don't have enough space
    if(buffer_position == buffer_size) {
        buffer_position--;
    }

    buffer[buffer_position] = '\0';

    return buffer_position;
}