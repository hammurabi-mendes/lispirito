#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lambdas.h"
#include "macros.h"

int main(int argc, char **argv) {
	for(int i = 0; i < NUMBER_INITIAL_LAMBDAS; i++) {
        fputs(lambdas[i], stdout);
        fputs("\n", stdout);
    }

	for(int i = 0; i < NUMBER_INITIAL_MACROS; i++) {
        fputs(macros[i], stdout);
        fputs("\n", stdout);
    }

    return EXIT_SUCCESS;
}
