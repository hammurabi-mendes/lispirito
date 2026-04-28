#include "c64_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cbm.h>

#define Allocate malloc
#define Deallocate free

#define STATUS_TIMEOUT            0x02
#define STATUS_EOF                0x40
#define STATUS_DEVICE_NOT_PRESENT 0x80

struct C64_FILE {
    unsigned char lfn;            // LFN: Last file number (sequential, where 0 denotes the input)
    unsigned char last_status;
    unsigned char eof;
    unsigned char error;
    unsigned char write;
};

static unsigned char default_device = 8;
static unsigned char default_sa = 2; // Secondary address (SA): type of drive access for the 1541 (2 = sequential)

static unsigned char current_lfn = 1;

static inline void update_status(C64_FILE *descriptor, unsigned char status) {
    descriptor->last_status = status;

    if(status & STATUS_EOF) {
        descriptor->eof = 1;
    }

    if(status & (STATUS_TIMEOUT | STATUS_DEVICE_NOT_PRESENT)) {
        descriptor->error = 1;
    }
}

inline char *get_name1541(const char *name, const char *mode) {
    static char name1541[16];

    // Append filename in lowercase
    int i = 0;

    for(const char *current = name; *current != '\0'; current++) {
        name1541[i++] = toupper(*current);
    }

    name1541[i] = '\0';

    // Append mode
    strcat(name1541, mode);

    return name1541;
}

void c64_fset(unsigned char device, unsigned char sa) {
    default_device = device;
    default_sa = sa;
}

C64_FILE *c64_fopen(const char *name, const char *mode) {
    if(!name || !mode || (mode[0] != 'r' && mode[0] != 'w')) {
        return nullptr;
    }

    C64_FILE *descriptor = (C64_FILE *) Allocate(sizeof(C64_FILE));

    if(!descriptor) {
        return nullptr;
    }

    descriptor->lfn = current_lfn;
    descriptor->last_status = 0;
    descriptor->eof = 0;
    descriptor->error = 0;
    descriptor->write = 0;

    descriptor->write = (mode[0] == 'w');

    // Set what file to open (SETNAM) and how to open it (2 = sequential)
    cbm_k_setnam(get_name1541(name, descriptor->write ? ",W" : ",R"));
    cbm_k_setlfs(descriptor->lfn, default_device, default_sa);

    // Perform the file open
    uint8_t result = cbm_k_open();

    if(result != 0) {
        Deallocate(descriptor);

        return nullptr;
    }

    // Update up the current LFN but skip zero if we just wrapped around
    current_lfn++;

    if(current_lfn == 0) {
        current_lfn++;
    }

    return descriptor;
}

int c64_fclose(C64_FILE *descriptor) {
    if(descriptor == nullptr) {
        return -1;
    }

    // Reset I/O to keyboard/screen
    cbm_k_clrch();

    // Perform the file close
    cbm_k_close(descriptor->lfn);
    
    Deallocate(descriptor);

    return 0;
}

char *c64_fgets(char *buffer, int size, C64_FILE *descriptor) {
    if((FILE *) descriptor == stdin) {
        return fgets(buffer, size, (FILE *) stdin);
    }

    if(!buffer || !descriptor || descriptor->eof) {
        return nullptr;
    }

    descriptor->error = 0;

    unsigned char status;
    unsigned char character_read;

    // Redirect I/O to a file
    status = cbm_k_chkin(descriptor->lfn);

    if(status != 0) {
        descriptor->last_status = status;
        descriptor->error = 1;

        // Reset I/O to keyboard/screen
        cbm_k_clrch();

        return nullptr;
    }

    int position = 0;

    while (position < size - 1) {
        // Read one character from the output channel
        character_read = cbm_k_chrin();

        // Reach status flags
        status = cbm_k_readst();

        if(character_read == '\r' || character_read == '\n') {
            buffer[position++] = '\n';

            if(status != 0) {
                update_status(descriptor, status);
            }

            break;
        }

        buffer[position++] = (char) character_read;

        if(status != 0) {
            update_status(descriptor, status);
            break;
        }
    }

    // Reset I/O to keyboard/screen
    cbm_k_clrch();

    // Null-terminate string
    buffer[position] = '\0';

    return (position > 0 ? buffer : nullptr);
}

int c64_fputs(const char *buffer, C64_FILE *descriptor) {
    if((FILE *) descriptor == stdout) {
        return fputs(buffer, stdout);
    }

    if(!buffer || !descriptor || !descriptor->write) {
        return -1;
    }

    descriptor->error = 0;

    // Redirect I/O to a file
    unsigned char status = cbm_k_chkout(descriptor->lfn);

    if(status != 0) {
        descriptor->last_status = status;
        descriptor->error = 1;

        // Reset I/O to keyboard/screen
        cbm_k_clrch();

        return -1;
    }

    while(*buffer) {
        // Write one character to the output channel
        cbm_k_chrout((unsigned char) *buffer);
        buffer++;

        // Reach status flags
        status = cbm_k_readst();

        if(status != 0) {
            update_status(descriptor, status);

            // Reset I/O to keyboard/screen
            cbm_k_clrch();
            return -1;
        }
    }

    // Reset I/O to keyboard/screen
    cbm_k_clrch();
    return 0;
}