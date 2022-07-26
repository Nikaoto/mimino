#ifndef _MIMINO_ARG_H
#define _MIMINO_ARG_H

// Tells parse_args() which arguments we want to parse
typedef struct {
    char short_arg; // with single dash
    char *long_arg; // with double dash
    char *err;      // returned error message
    char type;      // one of [bsr]

    // Boolean value set by parse_args().
    // Is set to 1 if the argument was present, otherwise it's 0.
    int bvalue;     

    // String value set by parse_args().
    // Only set for string types.
    char *value;    
} Argdef;

// raw is for arguments given without flags
#define ARGDEF_TYPE_BOOL   'b'
#define ARGDEF_TYPE_STRING 's'
#define ARGDEF_TYPE_RAW    'r'

int parse_args(int argc, char **argv, int argdefc, Argdef *argdefs);
void print_argdef(Argdef *);
void print_argdefs(Argdef *a, int count);

#endif // _MIMINO_ARG_H
