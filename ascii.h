#ifndef _MIMINO_ASCII_H
#define _MIMINO_ASCII_H

int is_upper_ascii(char c);
int is_alpha(char c);
int is_digit(char c);
int is_alnum(char c);
int is_hex(char c);
int needs_encoding(unsigned char c);

#endif // _MIMINO_ASCII_H
