int
is_upper_ascii(char c)
{
    return c >= 'A' && c <= 'Z';
}

int
is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int
is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int
is_alnum(char c)
{
    return is_digit(c) || is_alpha(c);
}

int
is_hex(char c)
{
    return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// https://tools.ietf.org/html/rfc3986#section-2.3
int
needs_encoding(unsigned char c)
{
    if (is_alnum(c)) return 0;

    switch (c) {
    case '-':
    case '.':
    case '_':
    case '~':
        return 0;
    }

    return 1;
}
