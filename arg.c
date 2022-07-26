#include <stdio.h>
#include "arg.h"

int
parse_args(int argc, char **argv, int argdefc, Argdef *argdefs)
{
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

    return 0;
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
