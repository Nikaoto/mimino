#ifndef _MIMINO_ASCII_H
#define _MIMINO_ASCII_H

/* inline int is_upper_ascii(char c); */
/* inline int is_alpha(char c); */
/* inline int is_digit(char c); */
/* inline int is_alnum(char c); */

inline int
is_upper_ascii(char c)
{
    return c >= 'A' && c <= 'Z';
}

inline int
is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int
is_digit(char c)
{
    return c >= '0' && c <= '9';
}

inline int
is_alnum(char c)
{
    return is_digit(c) || is_alpha(c);
}

#endif // _MIMINO_ASCII_H
