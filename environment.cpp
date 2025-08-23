#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lambdas.h"
#include "macros.h"

int main(int argc, char **argv) {
	for(int i = 0; i < NUMBER_INITIAL_LAMBDAS; i++) {
        fprintf(stdout, "(define %s %s)\n", lambda_names[i], lambda_strings[i]);
    }

	for(int i = 0; i < NUMBER_INITIAL_MACROS; i++) {
        fprintf(stdout, "(define %s %s)\n", macro_names[i], macro_strings[i]);
    }

    return EXIT_SUCCESS;
}
