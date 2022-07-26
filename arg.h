#ifndef _MIMINO_ARG_H
#define _MIMINO_ARG_H

// Tells parse_args() which arguments we want to parse
typedef struct {
    int id;         // for quick lookup & comparison(?)
    char short_arg; // with single dash
    char *long_arg; // with double dash
    char *err;      // returned error message
    char type;      // one of [ibfdsr]
    // TODO: add .found bool and tests
    union {
        int i;    // integer
        int b;    // boolean
        float f;  // float
        double d; // double
        char *s;  // string
    } value;
} Argdef;

// raw is for arguments given without flags
#define ARGDEF_TYPE_INT    'i'
#define ARGDEF_TYPE_BOOL   'b'
#define ARGDEF_TYPE_FLOAT  'f'
#define ARGDEF_TYPE_DOUBLE 'd'
#define ARGDEF_TYPE_STRING 's'
#define ARGDEF_TYPE_RAW    'r'

int parse_args(int argc, char **argv, int argdefc, Argdef *argdefs);
void print_argdef(Argdef *);

#endif // _MIMINO_ARG_H
