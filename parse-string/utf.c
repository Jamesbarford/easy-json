#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __bufput(b, i, c) ((b)[(*i)++] = (c))

/**
 * Encodes a codepoint into UTF-8 encoded character(s)
 * Stores result in buffer, incrementing the offset
 * */
static inline void
utf8Encode(char *buffer, unsigned int codepoint, size_t *offset)
{
    /* Check if the codepoint can be represented with a single byte (0-127) */
    if (codepoint <= 0x7F) {
        __bufput(buffer, offset, codepoint & 0xFF);
    } else if (codepoint <= 0x7FF) {
        /* For codepoints between 128 and 2047, encode using two bytes */
        __bufput(buffer, offset, 0xC0 | ((codepoint >> 6) & 0xFF));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        /* For codepoints between 2048 and 65535, encode using three bytes */
        __bufput(buffer, offset, 0xE0 | ((codepoint >> 12) & 0xFF));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 6) & 0x3F));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    } else {
        /* For codepoints above 65535, encode using four bytes */
        __bufput(buffer, offset, 0xE0 | ((codepoint >> 18) & 0xFF));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 12) & 0x3F));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 6) & 0x3F));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    }
}

static char *
convertUTF16toUTF8(const uint16_t *utf16_str)
{
    size_t utf16_len = 0;
    size_t utf8_len = 0;

    while (utf16_str[utf16_len])
        ++utf16_len;

    for (size_t i = 0; i < utf16_len; ++i) {
        uint16_t utf16_char = utf16_str[i];
        if (utf16_char < 0x80) {
            utf8_len += 1;
        } else if (utf16_char < 0x800) {
            utf8_len += 2;
        } else {
            utf8_len += 3;
        }
    }

    char *utf8_str = (char *)malloc(utf8_len + 1);
    if (!utf8_str)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < utf16_len; ++i) {
        utf8Encode(utf8_str, (uint16_t)utf16_str[i], &j);
    }
    utf8_str[utf8_len] = '\0';
    return utf8_str;
}

int
main(void)
{
    uint16_t utf16_str[] = {
        0x0077,
        0x0068,
        0x0061,
        0x0074,
        0x0020,
        0x0061,
        0x0020,
        0x0066,
        0x0061,
        0x0073,
        0x0074,
        0x0020,
        /* utf-16 chars */
        0x2222,
        0x4121,
        0x6711,
        0x0006,
        /* utf-16 chars end */
        /*--*/
        /* snail emoji */
        0xD83D,
        0xDC0C,
        /* snail emoji end */
        0x0021, /* '!' */
        0x0,
    };

    char *utf8_string = convertUTF16toUTF8(utf16_str);
    /* This does not print, not entirely sure why! */
    printf("UTF-16: %ls\n", (wchar_t *)utf16_str);
    printf("UTF-8 equivalent: %s\n", utf8_string);
    free(utf8_string);
    return 0;
}
