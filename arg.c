#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arg.h"

int debug = 0;

// Return 1 if the argument matches the short-style flag description
// (i.e: flag is 'b' and arg is '-b').
// Otherwise , return 0.
static int
short_arg_match(char *arg, char flag)
{
    if (arg[0] != '-') return 0;
    if (arg[1] == flag) return 1;
    return 0;
}

// Return 1 if the argument matches the long-style flag description
// (i.e: flag is 'file' and arg is '--file' or '--file=x')
static int
long_arg_match(char *arg, char *flag)
{
    if (arg[0] != '-') return 0;
    if (arg[1] != '-') return 0;
    arg += 2;

    while (1) {
        if (*arg == '\0')  break;
        if (*flag == '\0') break;
        if (*arg != *flag) return 0;
        arg++;
        flag++;
    }

    // flag is longer
    if (*flag != '\0') return 0;

    // Matched
    if (*arg == '\0' || *arg == '=') return 1;

    return 0;
}

int
parse_args(int argc, char **argv, int argdefc, Argdef *argdefs)
{
    int ret = 1;

    if (debug) {
        printf("parse_args() called:\n");

        printf("argc: %i\n", argc);
        printf("argv: [\n");
        for (int i = 0; i < argc; i++) {
            printf("%s\n", argv[i]);
        }
        printf("]\n");

        printf("argdefc: %i\n", argdefc);
        printf("argdefs: [\n");
        for (int i = 0; i < argdefc; i++) {
            print_argdef(argdefs + i);
        }
        printf("]\n");
    }


    int last_raw_arg_ind = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            // Long-form arg
            for (int j = 0; j < argdefc; j++) {
                if (long_arg_match(argv[i], argdefs[j].long_arg)) {
                    char *eqp = NULL;
                    switch (argdefs[j].type) {

                    case ARGDEF_TYPE_BOOL:
                        argdefs[j].value.b = 0;

                        // No '=' present
                        eqp = strchr(argv[i], '=');
                        if (!eqp) {
                            argdefs[j].value.b = 1;
                            break;
                        }

                        // '=' present. 0 or \0 is false, anything else is true
                        if (eqp[1] == '\0' || eqp[1] == '0') {
                            argdefs[j].value.b = 0;
                        } else {
                            argdefs[j].value.b = 1;
                        }
                        break;

                    case ARGDEF_TYPE_STRING:
                    case ARGDEF_TYPE_INT:
                    case ARGDEF_TYPE_FLOAT:
                    case ARGDEF_TYPE_DOUBLE:
                        argdefs[j].value.s = NULL;
                        eqp = strchr(argv[i], '=');

                        // No '=' present
                        if (!eqp) {
                            if (i + 1 >= argc) {
                                argdefs[i].err = "No value given for argument";
                                break;
                            }

                            argdefs[j].value.s = argv[i+1];
                            i++;
                            break;
                        }

                        // '=' present
                        if (eqp[1] == '\0') {
                            argdefs[j].value.s = NULL;
                            argdefs[j].err = "No value given after \"=\"";
                            ret = 0;
                        } else {
                            argdefs[j].value.s = eqp + 1;
                        }
                        break;
                    }

                    // We've parsed the curent argument, no need to check
                    // other argdefs
                    break;
                }
            }
        } else if (argv[i][0] == '-') {
            // Short-form arg
            int stop = 0;
            for (char *c = argv[i] + 1; *c != '\0' && !stop; c++) {
                for (int j = 0; j < argdefc; j++) {
                    if (argdefs[j].short_arg == *c) {
                        if (argdefs[j].type == ARGDEF_TYPE_BOOL) {
                            argdefs[j].value.b = 1;
                            break;
                        } else if (argdefs[j].type != ARGDEF_TYPE_RAW) {
                            // argdefs[j] is either int, string, float or double
                            argdefs[j].value.s = c + 1;
                            stop = 1;
                            break;
                        }
                    }
                }
            }
        } else {
            // Raw arg
            for (int j = last_raw_arg_ind; j < argdefc; j++) {
                if (argdefs[j].type == ARGDEF_TYPE_RAW) {
                    argdefs[j].value.s = argv[i];
                    last_raw_arg_ind = j + 1;
                    break;
                }
            }
        }
    }

    if (debug) {
        printf("first step finished, argdefs: [\n");
        for (int i = 0; i < argdefc; i++) {
            print_argdef(argdefs + i);
        }
        printf("]\n");
    }

    // TODO: step 2: iterate all int, float and double argdefs and parse their
    // string values

    if (debug) {
        printf("parse finished, argdefs: [\n");
        for (int i = 0; i < argdefc; i++) {
            print_argdef(argdefs + i);
        }
        printf("]\n");
    }

    return ret;
}

void
print_argdef(Argdef *a)
{
    if (!a) {
        printf("(Argdef) NULL\n");
        return;
    }

    printf("(Argdef) {\n");
    printf("  .id = %i\n", a->id);
    printf("  .short_arg = %c\n", a->short_arg);
    printf("  .long_arg = %s\n", a->long_arg);
    printf("  .err = %s\n", a->err);
    printf("  .type = %c\n", a->type);

    printf("  .value = ");
    switch (a->type) {
    case ARGDEF_TYPE_RAW:    printf("%s", a->value.s); break;
    case ARGDEF_TYPE_INT:    printf("%i", a->value.i); break;
    case ARGDEF_TYPE_BOOL:   printf("%i", a->value.b); break;
    case ARGDEF_TYPE_FLOAT:  printf("%g", a->value.f); break;
    case ARGDEF_TYPE_DOUBLE: printf("%g", a->value.d); break;
    case ARGDEF_TYPE_STRING: printf("%s", a->value.s); break;
    }
    printf("\n");

    printf("}\n");
}
