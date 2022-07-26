#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arg.h"

static int debug = 0;

// Return 1 if the argument matches the long-style flag description
// (i.e: flag is 'file' and arg is '--file' or '--file=x')
static int
long_arg_match(char *arg, char *flag)
{
    if (!arg) return 0;
    if (!flag) return 0;

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

static int
short_arg_match(char *arg, char *flag)
{
    if (!arg) return 0;
    if (!flag) return 0;

    return *arg == *flag;
}

int
parse_args(int argc, char **argv, int argdefc, Argdef *argdefs)
{
    int ret = 1;

    if (debug) {
        printf("argc: %i\n", argc);
        printf("argv: [\n");
        for (int i = 0; i < argc; i++) {
            printf("%s\n", argv[i]);
        }
        printf("]\n");

        print_argdefs(argdefs, argdefc);
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
                        argdefs[j].bvalue = 0;

                        // No '=' present
                        eqp = strchr(argv[i], '=');
                        if (!eqp) {
                            argdefs[j].bvalue = 1;
                            break;
                        }

                        // '=' present. 0 or \0 is false, anything else is true
                        if (eqp[1] == '\0' || eqp[1] == '0') {
                            argdefs[j].bvalue = 0;
                        } else {
                            argdefs[j].bvalue = 1;
                        }
                        break;

                    case ARGDEF_TYPE_STRING:
                        argdefs[j].bvalue = 1;

                        eqp = strchr(argv[i], '=');
                        // No '=' present
                        if (!eqp) {
                            if (i + 1 >= argc) {
                                argdefs[i].err = "No value given for argument";
                                ret = 0;
                                break;
                            }

                            argdefs[j].value = argv[i+1];
                            i++;
                            break;
                        }

                        // '=' present
                        if (eqp[1] == '\0') {
                            argdefs[j].value = NULL;
                            argdefs[j].err = "No value given after \"=\"";
                            ret = 0;
                        } else {
                            argdefs[j].value = eqp + 1;
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
                    if (short_arg_match(c, &(argdefs[j].short_arg))) {
                        argdefs[j].bvalue = 1;

                        if (argdefs[j].type == ARGDEF_TYPE_BOOL)
                            break;

                        if (argdefs[j].type == ARGDEF_TYPE_STRING) {
                            if (c[1] != '\0') {
                                // Argument written inline like '-owide',
                                // where the flag is '-o' and value is 'wide'
                                argdefs[j].value = c + 1;
                                stop = 1;
                                break;
                            }

                            // No value after flag
                            if (i + 1 >= argc) {
                                argdefs[j].err = "No value given for argument";
                                ret = 0;
                                break;
                            }

                            // Pick up next value
                            argdefs[j].value = argv[i + 1];
                            i++;
                            break;
                        }
                    }
                }
            }
        } else {
            // Raw arg
            for (int j = last_raw_arg_ind; j < argdefc; j++) {
                if (argdefs[j].type == ARGDEF_TYPE_RAW) {
                    argdefs[j].value = argv[i];
                    last_raw_arg_ind = j + 1;
                    break;
                }
            }
        }
    }

    if (debug) print_argdefs(argdefs, argdefc);

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
    printf("  .short_arg = %c\n", a->short_arg);
    printf("  .long_arg = %s\n", a->long_arg);
    printf("  .err = \"%s\"\n", a->err);
    printf("  .type = %c\n", a->type);
    printf("  .bvalue = %d\n", a->bvalue);
    printf("  .value = \"%s\"\n", a->value);
    printf("}\n");
}

void
print_argdefs(Argdef *a, int count)
{
    printf("(Argdef[%d]) [\n", count);
    for (int i = 0; i < count; i++)
        print_argdef(a + i);
    printf("]\n");
}
