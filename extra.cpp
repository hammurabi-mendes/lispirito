#ifndef PRINT_HPP
#define PRINT_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extra.h"

void print_integral(Integral n) {
    char buffer[128];

    get_integral_string(n, buffer);

    fputs(buffer, stdout);
}

void get_integral_string(Integral n, char *buffer) {
    int position = 0;

    if(n == 0) {
        buffer[position] = '0';
        position++;

        buffer[position] = '\0';
        return;
    }
    
    if(n < 0) {
        buffer[position] = '-';
        position++;

        n = -n;
    }
    
    Integral reversed = 0;
    int digit_count = 0;
    
    while(n > 0) {
        reversed = (reversed * 10) + (n % 10);
        n /= 10;

        digit_count++;
    }
    
    while(digit_count--) {
        buffer[position] = '0' + (reversed % 10);
        position++;

        reversed /= 10;
    }

    buffer[position] = '\0';
    return;
}

void print_real(Real f) {
    char buffer[128];

    get_real_string(f, buffer);

    fputs(buffer, stdout);
}

#ifdef TARGET_6502
char *strdup(const char *input) {
    int input_length = strlen(input);

    char *result = (char *) malloc(input_length + 1);

    for(int i = 0; i < input_length; i++) {
        result[i] = input[i];
    }

    result[input_length] = '\0';

    return result;
}

Real atof(char *input) {
    char *integral = input;
    char *fraction = nullptr;

    for(int i = 0; input[i] != '\0'; i++) {
        if(input[i] == '.') {
            input[i] = '\0';
            fraction = input + (i + 1);
        }
    }

    Real result(atol(integral), atol(fraction));

    return result;
}

void get_real_string(Real f, char *buffer) {
    buffer[0] = '\0';

    char buffer_internal[128];

    get_real_string(f.as_i(), buffer_internal);
    strcat(buffer, buffer_internal);

    strcat(buffer, ".");

    get_real_string(f.as_f(), buffer_internal);
    strcat(buffer, buffer_internal);
}
#else
void get_real_string(Real f, char *buffer) {
    int position = 0;

    if(f < 0) {
        buffer[position] = '-';
        position++;

        f = -f;
    }

    Integral integral_part = static_cast<Integral>(f);
    Integral fractional_part = ((f - (Real) integral_part) * 1000000);

    buffer[position] = '\0';

    char buffer_internal[128];

    get_integral_string(integral_part, buffer_internal);
    strcat(buffer, buffer_internal);

    strcat(buffer, ".");

    get_integral_string(fractional_part, buffer_internal);
    strcat(buffer, buffer_internal);
}
#endif /* TARGET_6502 */

#endif /* PRINT_HPP */