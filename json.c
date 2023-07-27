#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define __bufput(b, i, c) ((b)[(*i)++] = (c))

#define isWhiteSpace(ch)                                                  \
    (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || \
     ch == '\r')
#define isNum(ch) ((ch) >= '0' && (ch) <= '9')
#define isHex(ch) \
    (isNum(ch) || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F')
#define toInt(ch)           (ch - '0')
#define toUpper(ch)         ((ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch)
#define toHex(ch)           (toUpper(ch) - 'A' + 10)
#define isNumTerminator(ch) (ch == ',' || ch == ']' || ch == '\0' || ch == '\n')
#define numStart(ch)        (isNum(ch) || ch == '-' || ch == '+' || ch == '.')

#define json_debug(...)                                                    \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

#define json_debug_panic(...)                                              \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
        exit(EXIT_FAILURE);                                                \
    } while (0)

typedef enum JsonParserType {
    JSON_PARSER_STRING,
    JSON_PARSER_NUMERIC,
    JSON_PARSER_ARRAY,
    JSON_PARSER_OBJECT,
    JSON_PARSER_BOOL,
    JSON_PARSER_NULL,
} JsonParserType;

typedef struct jsonParser {
    /* data type being parsed */
    JsonParserType type;
    /* buffer containing json string */
    char *buffer;
    /* current position in the buffer */
    size_t offset;
    /* length of the buffer */
    size_t buflen;
    /* error code when failing to parse the buffer */
    JSON_ERRNO errno;
    /* pointer to the root of the json object that represents
     * the data in the buffer. */
    json *J;
    /* pointer to the current node in the json object that is being parsed.
     * */
    json *ptr;
    /* Parsing flags */
    int flags;
    /* State of the parser and the resulting state of the parsed json when
     * finished */
    jsonState *state;
} jsonParser;

static jsonState json_global_state;

static jsonState *jsonStateNew(void) {
    jsonState *json_state = malloc(sizeof(jsonState));
    json_state->error = JSON_OK;
    json_state->ch = '\0';
    json_state->offset = 0;
    return json_state;
}

static const double powers[] = {
        1e+0,   1e+1,   1e+2,   1e+3,   1e+4,   1e+5,   1e+6,   1e+7,   1e+8,
        1e+9,   1e+10,  1e+11,  1e+12,  1e+13,  1e+14,  1e+15,  1e+16,  1e+17,
        1e+18,  1e+19,  1e+20,  1e+21,  1e+22,  1e+23,  1e+24,  1e+25,  1e+26,
        1e+27,  1e+28,  1e+29,  1e+30,  1e+31,  1e+32,  1e+33,  1e+34,  1e+35,
        1e+36,  1e+37,  1e+38,  1e+39,  1e+40,  1e+41,  1e+42,  1e+43,  1e+44,
        1e+45,  1e+46,  1e+47,  1e+48,  1e+49,  1e+50,  1e+51,  1e+52,  1e+53,
        1e+54,  1e+55,  1e+56,  1e+57,  1e+58,  1e+59,  1e+60,  1e+61,  1e+62,
        1e+63,  1e+64,  1e+65,  1e+66,  1e+67,  1e+68,  1e+69,  1e+70,  1e+71,
        1e+72,  1e+73,  1e+74,  1e+75,  1e+76,  1e+77,  1e+78,  1e+79,  1e+80,
        1e+81,  1e+82,  1e+83,  1e+84,  1e+85,  1e+86,  1e+87,  1e+88,  1e+89,
        1e+90,  1e+91,  1e+92,  1e+93,  1e+94,  1e+95,  1e+96,  1e+97,  1e+98,
        1e+99,  1e+100, 1e+101, 1e+102, 1e+103, 1e+104, 1e+105, 1e+106, 1e+107,
        1e+108, 1e+109, 1e+110, 1e+111, 1e+112, 1e+113, 1e+114, 1e+115, 1e+116,
        1e+117, 1e+118, 1e+119, 1e+120, 1e+121, 1e+122, 1e+123, 1e+124, 1e+125,
        1e+126, 1e+127, 1e+128, 1e+129, 1e+130, 1e+131, 1e+132, 1e+133, 1e+134,
        1e+135, 1e+136, 1e+137, 1e+138, 1e+139, 1e+140, 1e+141, 1e+142, 1e+143,
        1e+144, 1e+145, 1e+146, 1e+147, 1e+148, 1e+149, 1e+150, 1e+151, 1e+152,
        1e+153, 1e+154, 1e+155, 1e+156, 1e+157, 1e+158, 1e+159, 1e+160, 1e+161,
        1e+162, 1e+163, 1e+164, 1e+165, 1e+166, 1e+167, 1e+168, 1e+169, 1e+170,
        1e+171, 1e+172, 1e+173, 1e+174, 1e+175, 1e+176, 1e+177, 1e+178, 1e+179,
        1e+180, 1e+181, 1e+182, 1e+183, 1e+184, 1e+185, 1e+186, 1e+187, 1e+188,
        1e+189, 1e+190, 1e+191, 1e+192, 1e+193, 1e+194, 1e+195, 1e+196, 1e+197,
        1e+198, 1e+199, 1e+200, 1e+201, 1e+202, 1e+203, 1e+204, 1e+205, 1e+206,
        1e+207, 1e+208, 1e+209, 1e+210, 1e+211, 1e+212, 1e+213, 1e+214, 1e+215,
        1e+216, 1e+217, 1e+218, 1e+219, 1e+220, 1e+221, 1e+222, 1e+223, 1e+224,
        1e+225, 1e+226, 1e+227, 1e+228, 1e+229, 1e+230, 1e+231, 1e+232, 1e+233,
        1e+234, 1e+235, 1e+236, 1e+237, 1e+238, 1e+239, 1e+240, 1e+241, 1e+242,
        1e+243, 1e+244, 1e+245, 1e+246, 1e+247, 1e+248, 1e+249, 1e+250, 1e+251,
        1e+252, 1e+253, 1e+254, 1e+255, 1e+256, 1e+257, 1e+258, 1e+259, 1e+260,
        1e+261, 1e+262, 1e+263, 1e+264, 1e+265, 1e+266, 1e+267, 1e+268, 1e+269,
        1e+270, 1e+271, 1e+272, 1e+273, 1e+274, 1e+275, 1e+276, 1e+277, 1e+278,
        1e+279, 1e+280, 1e+281, 1e+282, 1e+283, 1e+284, 1e+285, 1e+286, 1e+287,
        1e+288, 1e+289, 1e+290, 1e+291, 1e+292, 1e+293, 1e+294, 1e+295, 1e+296,
        1e+297, 1e+298, 1e+299, 1e+300, 1e+301, 1e+302, 1e+303, 1e+304, 1e+305,
        1e+306, 1e+307, 1e+308,
};

/* Retrun power of 10 from the cache */
static double I64Pow10(int idx) {
    if (idx > 308) {
        return idx;
    } else if (idx < -308) {
        return 0.0;
    }
    return powers[idx];
}

/**
 * Use SMID if avalible for machine, this is the one I have on my
 * computer hence the one I've implemented. Makes moving past
 * whitespace faster.
 */
#if defined(__SSE2__)
#include <emmintrin.h>
static size_t getNextNonWhitespaceIdx(const char *ptr) {
    char *start = (char *)ptr;
    if (isWhiteSpace(*ptr)) {
        ++ptr;
    } else {
        return 0;
    }

    /* Find next byte alignment;
     * the next boundary for the next 16-byte aligned block
     */
    const char *next_boundary = (const char *)((((size_t)ptr) + 15) &
                                               ((size_t)ULLONG_MAX - 15));

    /* loop through characters until the next boundary, checking for whitespaces
     */
    while (ptr != next_boundary) {
        if (isWhiteSpace(*ptr)) {
            ++ptr;
        } else {
            return ptr - start;
        }
    }

    static const char whitespaces[4][16] = {
            {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
             ' ', ' ', ' '},
            {'\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
             '\n', '\n', '\n', '\n', '\n'},
            {'\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
             '\r', '\r', '\r', '\r', '\r'},
            {'\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
             '\t', '\t', '\t', '\t', '\t'},
    };

    const __m128i w0 = _mm_loadu_si128((const __m128i *)(&whitespaces[0][0]));
    const __m128i w1 = _mm_loadu_si128((const __m128i *)(&whitespaces[1][0]));
    const __m128i w2 = _mm_loadu_si128((const __m128i *)(&whitespaces[2][0]));
    const __m128i w3 = _mm_loadu_si128((const __m128i *)(&whitespaces[3][0]));

    /* Find next non-whitespace character 16 characters at a time */
    while (1) {
        /* Load 16 bytes from the input string into an __m128i variable */
        const __m128i s = _mm_load_si128((const __m128i *)(ptr));

        /* Compare chunk of 16 characters to whitespace array */
        __m128i x = _mm_cmpeq_epi8(s, w0);
        x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w1));
        x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w2));
        x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w3));

        /* Use _mm_movemask_epi8 to get a mask indicating which bytes matched
         * this returns a 16bit integer where the bits are set to 1 if there
         * was a match. We just need it to be non-zero
         */
        unsigned short r = (unsigned short)(~_mm_movemask_epi8(x));
        /* If any of the bytes match, return the position of the first match */
        if (r != 0) {
            /* use __builtin_fss to find the first set bit in the mask (non
             * white space character) */
            return (ptr + __builtin_ffs(r) - 1) - start;
        }
        ptr += 16;
    }
}

#else
/* Much simpler niave version */
static size_t getNextNonWhitespaceIdx(const char *ptr) {
    char *start = (char *)ptr;
    while (isWhiteSpace(*ptr)) {
        ++ptr;
    }
    return ptr - start;
}
#endif

/**
 * Create new json object
 */
static json *jsonNew(void) {
    json *J = malloc(sizeof(json));
    J->type = JSON_NULL;
    J->key = NULL;
    J->next = NULL;
    J->state = NULL;
    return J;
}

/**
 * Needs to be null checked before calling this function
 */
static char jsonUnsafePeekAt(jsonParser *p, size_t idx) {
    return p->buffer[idx];
}

/**
 * Look at the current character
 */
static char jsonPeek(jsonParser *p) {
    return p->buffer[p->offset];
}

/**
 * Check if we can advance the buffer by 'jmp' characters
 */
static int jsonCanAdvanceBy(jsonParser *p, size_t jmp) {
    return p->offset + jmp < p->buflen;
}

/**
 * Advance the offset into the buffer by 'jmp'
 */
static void jsonUnsafeAdvanceBy(jsonParser *p, size_t jmp) {
    p->offset += jmp;
}

/**
 * Advance to error location and return 0
 */
static int jsonAdvanceToError(jsonParser *p, size_t jmp, int error_code) {
    p->errno = error_code;
    jsonUnsafeAdvanceBy(p, jmp);
    return 0;
}

/**
 * Advance buffers offset
 */
static void jsonAdvance(jsonParser *p) {
    if (jsonCanAdvanceBy(p, 1)) {
        ++p->offset;
        return;
    }
    p->errno = JSON_EOF;
}

/**
 * Advance to termintor
 */
static void jsonAdvanceToTerminator(jsonParser *p, char terminator) {
    while (jsonCanAdvanceBy(p, 1) && jsonPeek(p) != terminator) {
        ++p->offset;
    }
}

/**
 * Advance past whitespace characters
 */
static void jsonAdvanceWhitespace(jsonParser *p) {
    p->offset += getNextNonWhitespaceIdx(p->buffer + p->offset);
}

/**
 * Json parser ready to rock and roll
 */
static void jsonParserInit(jsonParser *p, char *buffer, size_t buflen) {
    p->offset = 0;
    p->buffer = buffer;
    p->buflen = buflen;
    p->type = -1;
    p->J = NULL;
    p->ptr = NULL;
    p->errno = JSON_OK;
}

/* All prototypes for parsing */
static json *jsonParseObject(jsonParser *p);
static json *jsonParseArray(jsonParser *p);
static int jsonParseBool(jsonParser *p);
static int jsonParseValue(jsonParser *p);
static int jsonParseNull(jsonParser *p);
static char *jsonParseString(jsonParser *p);
static void jsonParseNumber(jsonParser *p);

/* Number parsing */

/* Takes a string pointer and a pointer to an integer as input, and returns the
 * number of decimal numeric characters in the string, and the position of the
 * decimal point if present.
 */
static int countMantissa(char *ptr, int *decidx) {
    int mantissa = 0;
    *decidx = -1;

    for (;; ++mantissa) {
        char ch = *ptr;
        /* If the character is not a decimal numeric character, or if
         * the decimal point has already been found exit the loop
         */
        if (!isNum(ch)) {
            if ((ch != '.') || *decidx >= 0) {
                break;
            }
            *decidx = mantissa;
        }
        ptr++;
    }
    return mantissa;
}

/* Get index where the number hits a terminal character &
 * a basic check for the validity of a number */
static int countNumberLen(jsonParser *p, char *ptr) {
    char *start = ptr;
    int seen_e = 0;
    int seen_dec = 0;
    int seen_X = 0;

    while (!isNumTerminator(*ptr)) {
        switch (*ptr) {
        case 'e':
        case 'E':
            if (seen_e) {
                return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
            }
            seen_e = 1;
            break;
        case 'x':
        case 'X':
            if (seen_X) {
                return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
            }
            seen_X = 1;
            break;
        case '-':
        case '+':
            break;

        case '.':
            if (seen_dec) {
                return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
            }
            seen_dec = 1;
            break;
        /* Anything else is invalid */
        default:
            if (!isHex(*ptr)) {
                return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
            }
            break;
        }
        ptr++;
    }

    /* Floating point hex does not exist */
    if (seen_dec && seen_X) {
        return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
    }

    /* Exponent hex does not exist */
    if (seen_X && seen_e) {
        return jsonAdvanceToError(p, ptr - start, JSON_INVALID_NUMBER);
    }

    return ptr - start;
}

static long stringToHex(jsonParser *p) {
    long retval = 0;

    while (!isNumTerminator(jsonPeek(p))) {
        char ch = jsonPeek(p);
        if (isNum(ch)) {
            retval = retval * 16 + toInt(ch);
        } else if (isHex(ch)) {
            retval = retval * 16 + toHex(ch);
        } else {
            p->errno = JSON_INVALID_HEX;
            break;
        }
        jsonUnsafeAdvanceBy(p, 1);
    }

    return retval;
}

static ssize_t stringToI64(jsonParser *p, int neg) {
    ssize_t retval = 0;
    char cur;
    int exponent = 0;
    int exponent_sign = 0;

    if (jsonPeek(p) == '0') {
        jsonUnsafeAdvanceBy(p, 1);
        if (toUpper(jsonPeek(p)) == 'X') {
            jsonUnsafeAdvanceBy(p, 1);
            return stringToHex(p);
        } else {
            return jsonAdvanceToError(p, 0, JSON_INVALID_NUMBER);
        }
    }

    while (1) {
        cur = jsonPeek(p);
        switch (cur) {
        case 'e':
        case 'E':
            goto parse_exponent;

        case ',':
        case '\n':
        case ']':
        case '\0':
            goto out;

        default:
            retval = retval * 10 + toInt(cur);
            break;
        }
        jsonUnsafeAdvanceBy(p, 1);
    }

parse_exponent:
    if (toUpper(jsonPeek(p)) == 'E') {
        jsonUnsafeAdvanceBy(p, 1);
        if (jsonPeek(p) == '-') {
            exponent_sign = 1;
            jsonUnsafeAdvanceBy(p, 1);
        } else if (jsonPeek(p) == '+') {
            jsonUnsafeAdvanceBy(p, 1);
        }
        while (isNum(jsonPeek(p)) && exponent < INT_MAX / 100) {
            exponent = exponent * 10 + toInt(jsonPeek(p));
            jsonUnsafeAdvanceBy(p, 1);
        }
        while (isNum(jsonPeek(p))) {
            jsonUnsafeAdvanceBy(p, 1);
        }
    }

    if (exponent && exponent_sign) {
        retval /= I64Pow10(exponent);
    } else if (exponent) {
        retval *= I64Pow10(exponent);
    }

out:
    if (neg) {
        return -retval;
    }
    return retval;
}

static double jsonParseFloat(jsonParser *p, char *ptr, int neg, int num_len,
                             int dec_idx, int mantissa) {
    int fraction_exponent = 0, exponent = 0, exponent_sign = 0;

    if (mantissa == 0) {
        jsonUnsafeAdvanceBy(p, num_len);
        return 0.0;
    }

    if (dec_idx < 0) {
        dec_idx = mantissa;
    } else {
        mantissa -= 1;
    }

    if (mantissa > 18) {
        fraction_exponent = dec_idx - 18;
        mantissa = 18;
    } else {
        fraction_exponent = dec_idx - mantissa;
    }

    int ii = 0;
    int jj = 0;
    double fraction = 0;
    for (; mantissa > 9; --mantissa) {
        char c = *ptr;
        ptr++;
        if (c == '.') {
            c = *ptr;
            ptr++;
        }
        ii = 10 * ii + toInt(c);
    }

    for (; mantissa > 0; --mantissa) {
        char c = *ptr;
        ptr++;
        if (c == '.') {
            c = *ptr;
            ptr++;
        }
        jj = 10 * jj + toInt(c);
    }
    fraction = (1.0e9 * ii) + jj;

    if (toUpper(*ptr) == 'E') {
        ptr++;
        if (*ptr == '-') {
            exponent_sign = -1;
            ptr++;
        } else if (*ptr == '+') {
            ptr++;
        }
        while (isNum(*ptr) && exponent < INT_MAX / 100) {
            exponent = exponent * 10 + (*ptr - '0');
            ptr++;
        }
        while (isNum(*ptr)) {
            ptr++;
        }
    }

    if (exponent_sign) {
        exponent = fraction_exponent - exponent;
    } else {
        exponent = fraction_exponent + exponent;
    }

    if (exponent < -JSON_MAX_EXPONENT) {
        exponent = JSON_MAX_EXPONENT;
        exponent_sign = 1;
    } else if (exponent > JSON_MAX_EXPONENT) {
        exponent = JSON_MAX_EXPONENT;
        exponent_sign = 0;
    } else if (exponent < 0) {
        exponent_sign = 1;
        exponent = -exponent;
    } else {
        exponent_sign = 0;
    }

    double double_exponent = I64Pow10(exponent);
    double retval = fraction;

    if (exponent_sign) {
        retval /= double_exponent;
    } else {
        retval *= double_exponent;
    }

    jsonUnsafeAdvanceBy(p, num_len);
    if (neg) {
        return -retval;
    }
    return retval;
}

static void jsonParseNumber(jsonParser *p) {
    char *ptr = p->buffer + p->offset;
    int num_len = countNumberLen(p, ptr);
    json *current = p->ptr;
    int neg = 0;
    int dec_idx = -1;
    int mantissa = 0;

    if (*ptr == '-') {
        neg = 1;
        ptr++;
    } else if (*ptr == '+') {
        neg = 0;
        ptr++;
    } else if ((!isNum(*ptr) && *ptr != '.') || (num_len == 0)) {
        jsonAdvanceToError(p, 0, JSON_INVALID_NUMBER);
        return;
    }

    if (num_len == 1) {
        current->integer = toInt(jsonPeek(p));
        current->type = JSON_INT;
        jsonAdvance(p);
        return;
    }

    mantissa = countMantissa(ptr, &dec_idx);

    if (dec_idx == -1) {
        if (neg) {
            jsonAdvance(p);
        }
        current->integer = stringToI64(p, neg);
        current->type = JSON_INT;
        return;
    }

    current->floating = jsonParseFloat(p, ptr, neg, num_len, dec_idx, mantissa);
    current->type = JSON_FLOAT;
}

/**
 * Convert first 4 characters of buf to a decimal
 * representation
 */
static unsigned int parseHex4(const unsigned char *buf) {
    unsigned int hex = 0;
    unsigned char ch;

    for (int i = 0; i < 4; ++i) {
        ch = buf[i];
        if (isHex(ch)) {
            if (isHex(ch)) {
                hex += toHex(ch);
            } else {
                hex += toInt(ch);
            }
        } else {
            return INT_MAX;
        }
        if (i < 3) {
            hex = hex << 4;
        }
    }

    return hex;
}

/**
 * Encodes a codepoint into UTF-8 encoded character(s)
 * Stores result in buffer, incrementing the offset
 * */
static void utf8Encode(char *buffer, unsigned int codepoint, size_t *offset) {
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

static unsigned int jsonParseUTF16(jsonParser *p) {
    if (!jsonCanAdvanceBy(p, 5)) {
        return jsonAdvanceToError(p, 0, JSON_CANNOT_ADVANCE);
    }
    unsigned int codepoint = parseHex4((const unsigned char *)p->buffer +
                                       p->offset + 1);

    if (codepoint == INT_MAX) {
        return jsonAdvanceToError(p, 0, JSON_INVALID_HEX);
    }

    /* 5 -> 'u1111' is 5 chars */
    jsonUnsafeAdvanceBy(p, 4);

    /* UTF-16 pair */
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        if (codepoint <= 0xDBFF) {
            if (!jsonCanAdvanceBy(p, 6)) {
                return jsonAdvanceToError(p, 0, JSON_CANNOT_ADVANCE);
            }

            jsonAdvance(p);
            if (jsonPeek(p) != '\\' &&
                jsonUnsafePeekAt(p, p->offset + 1) != 'u') {
                return jsonAdvanceToError(p, 0, JSON_INVALID_UTF16);
            }

            jsonUnsafeAdvanceBy(p, 2);

            unsigned int codepoint2 = parseHex4(
                    (const unsigned char *)p->buffer + p->offset);
            if (codepoint2 == INT_MAX) {
                return jsonAdvanceToError(p, 0, JSON_INVALID_HEX);
            }

            /* Invalid surrogate */
            if (codepoint2 < 0xDC00 || codepoint2 > 0xDFFF) {
                return jsonAdvanceToError(p, 0, JSON_INVALID_UTF16_SURROGATE);
            }
            codepoint = 0x10000 +
                    (((codepoint & 0x3FF) << 10) | (codepoint2 & 0x3FF));

            /*
            codepoint = (((codepoint - 0xD800) << 10) | (codepoint2 - 0xDC00)) +
                    0x10000;
            */
            jsonCanAdvanceBy(p, 3);
        } else {
            return jsonAdvanceToError(p, 0, JSON_INVALID_UTF16);
        }
    }

    return codepoint;
}

static char *jsonParseString(jsonParser *p) {
    int run = 1;
    size_t start = p->offset;
    size_t end = p->offset;

    if (jsonPeek(p) == '"') {
        jsonAdvance(p);
        end++;
    }

    /* approximate string length */
    while (jsonUnsafePeekAt(p, end) != '\0') {
        char cur = jsonUnsafePeekAt(p, end);
        if (cur == '"') {
            if (jsonUnsafePeekAt(p, end - 1) != '\\') {
                break;
            }
        } else if (cur == '\\') {
            // escape sequence so add another byte
            if (jsonUnsafePeekAt(p, end + 1) == '\0') {
                break;
            }
            end++;
        }
        end++;
    }

    if (jsonUnsafePeekAt(p, end) == '\0') {
        p->errno = JSON_EOF;
        return NULL;
    }

    size_t len = 0;
    char *str = malloc(sizeof(char) * end - start);

    while (run && jsonPeek(p) != '\0') {
        switch (jsonPeek(p)) {
        case '\\':
            jsonAdvance(p);
            switch (jsonPeek(p)) {
            case '\\':
            case '"':
            case '/':
                __bufput(str, &len, jsonPeek(p));
                break;
            case 'b':
                __bufput(str, &len, '\b');
                break;
            case 'f':
                __bufput(str, &len, '\f');
                break;
            case 'n':
                __bufput(str, &len, '\n');
                break;
            case 'r':
                __bufput(str, &len, '\r');
                break;
            case 't':
                __bufput(str, &len, '\t');
                break;
            case 'u': {
                unsigned int codepoint = jsonParseUTF16(p);
                if (p->errno != JSON_OK) {
                    goto err;
                }
                utf8Encode(str, codepoint, &len);
                break;
            }
            }
            break;
        case '"':
            run = 0;
            break;
        default:
            __bufput(str, &len, jsonPeek(p));
            break;
        }
        jsonAdvance(p);
    }

    if (jsonPeek(p) == '"') {
        p->errno = JSON_INVALID_STRING_NOT_TERMINATED;
        goto err;
    }

    str[len] = '\0';
    return str;

err:
    if (str) {
        free(str);
    }
    return NULL;
}

/**
 * Parse json boolean
 * 1: true
 * 0: false
 * -1: error
 */
static int jsonParseBool(jsonParser *p) {
    char peek = jsonPeek(p);
    int retval = -1;

    /* Check if the next character in the buffer is 't' for true */
    if (peek == 't') {
        if (!jsonCanAdvanceBy(p, 4)) {
            p->errno = JSON_CANNOT_ADVANCE;
            return -1;
        }
        /* check next 3 characters for 'rue' */
        retval = strncmp(p->buffer + p->offset + 1, "rue", 3) == 0 ? 1 : -1;
        jsonUnsafeAdvanceBy(p, 4);
    }
    /* Check if the next character in the buffer is 'f' for false */
    else if (peek == 'f') {
        if (!jsonCanAdvanceBy(p, 5)) {
            p->errno = JSON_CANNOT_ADVANCE;
            return -1;
        }
        /* check next 4 characters for 'alse' */
        retval = strncmp(p->buffer + p->offset + 1, "alse", 4) == 0 ? 0 : -1;
        jsonUnsafeAdvanceBy(p, 5);
    }

    /* Failed to parse boolean */
    if (retval == -1) {
        p->errno = JSON_CANNOT_ADVANCE;
    }
    return retval;
}

/**
 * Parse json null
 * 1: success
 * -1: error
 */
static int jsonParseNull(jsonParser *p) {
    if (!jsonCanAdvanceBy(p, 4)) {
        p->errno = JSON_CANNOT_ADVANCE;
        return -1;
    }
    int retval = strncmp(p->buffer + p->offset, "null", 4) == 0 ? 1 : -1;
    jsonUnsafeAdvanceBy(p, 4);
    return retval;
}

/**
 * Given a small amount of starting characters that JSON can be;
 * set what type is ready for the parser
 */
static int jsonSetExpectedType(jsonParser *p) {
    char peek = jsonPeek(p);

    switch (peek) {
    case '{':
        p->type = JSON_PARSER_OBJECT;
        break;

    case '[':
        p->type = JSON_PARSER_ARRAY;
        break;

    case 'n':
        p->type = JSON_PARSER_NULL;
        break;

    case 't':
    case 'f':
        p->type = JSON_PARSER_BOOL;
        break;

    case '"':
        p->type = JSON_PARSER_STRING;
        break;

    default: {
        if (numStart(peek)) {
            p->type = JSON_PARSER_NUMERIC;
            break;
        } else {
            p->errno = JSON_INVALID_JSON_TYPE_CHAR;
            return 0;
        }
    }
    }

    return 1;
}

/**
 * Parse a json object starting with '{'
 * of return NULL if the next character is '}'
 */
static json *jsonParseObject(jsonParser *p) {
    /* move past '{' */
    jsonAdvance(p);
    jsonAdvanceWhitespace(p);

    /* Object is empty we can skip */
    if (jsonPeek(p) == '}') {
        jsonAdvance(p);
        return NULL;
    }

    json *J;
    json *val = jsonNew();
    p->ptr = val;

    while (1) {
        J = p->ptr;
        jsonAdvanceToTerminator(p, '"');
        J->key = jsonParseString(p);
        jsonAdvanceToTerminator(p, ':');
        jsonAdvance(p);
        jsonAdvanceWhitespace(p);

        if (!jsonSetExpectedType(p) || !jsonParseValue(p)) {
            break;
        }

        jsonAdvanceWhitespace(p);
        if (jsonPeek(p) != ',') {
            if (jsonPeek(p) == '}' && !jsonCanAdvanceBy(p, 1)) {
                break;
            }
            jsonAdvance(p);
            break;
        }

        J->next = jsonNew();
        p->ptr = J->next;
    }

    return val;
}

/**
 * Very similar to parsing an object where starting char is '[',
 * if array is empty return NULL
 */
static json *jsonParseArray(jsonParser *p) {
    /* move past '[' */
    jsonAdvance(p);
    jsonAdvanceWhitespace(p);

    /* array empty we can skip */
    if (jsonPeek(p) == ']') {
        jsonAdvance(p);
        return NULL;
    }

    json *J;
    json *val = jsonNew();
    p->ptr = val;

    while (1) {
        J = p->ptr;
        jsonAdvanceWhitespace(p);

        if (!jsonSetExpectedType(p) || !jsonParseValue(p)) {
            break;
        }

        jsonAdvanceWhitespace(p);
        if (jsonPeek(p) != ',') {
            if (jsonPeek(p) == ']' && !jsonCanAdvanceBy(p, 1)) {
                break;
            }
            jsonAdvance(p);
            break;
        }

        jsonAdvance(p);
        J->next = jsonNew();
        p->ptr = J->next;
    }

    p->ptr = val;
    return val;
}

/**
 * Selects which parser to use depending on the type which has been set on the
 * previous parse
 */
static int jsonParseValue(jsonParser *p) {
    json *J = p->ptr;

    switch (p->type) {
    case JSON_PARSER_NUMERIC: {
        if (p->flags & JSON_STRNUM_FLAG) {
            J->type = JSON_STRNUM;
            J->strnum = jsonParseString(p);
        } else {
            jsonParseNumber(p);
        }
        break;
    }

    case JSON_PARSER_STRING:
        J->type = JSON_STRING;
        J->str = jsonParseString(p);
        break;

    case JSON_PARSER_NULL:
        if (jsonParseNull(p)) {
            J->type = JSON_NULL;
        }
        break;

    case JSON_PARSER_OBJECT:
        J->type = JSON_OBJECT;
        J->object = jsonParseObject(p);
        break;

    case JSON_PARSER_BOOL:
        J->type = JSON_BOOL;
        J->boolean = jsonParseBool(p);
        break;

    case JSON_PARSER_ARRAY:
        J->type = JSON_ARRAY;
        J->array = jsonParseArray(p);
        break;
    }

    return p->errno == JSON_OK;
}

static void printDepth(int depth) {
    for (int i = 0; i < depth - 1; ++i) {
        printf("  ");
    }
}

static unsigned char *escapeString(char *buf) {
    unsigned char *outbuf = NULL;
    unsigned char *ptr = (unsigned char *)buf;
    size_t len = 0;
    size_t escape_chars = 0;
    size_t offset = 0;

    if (buf == NULL) {
        return NULL;
    }

    while (*ptr) {
        switch (*ptr) {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            escape_chars++;
            break;
        default:
            if (*ptr < 32) {
                escape_chars += 6;
            }
            break;
        }
        ++ptr;
    }

    len = (size_t)(ptr - (unsigned char *)buf) + escape_chars;
    outbuf = malloc(sizeof(char) * len);

    if (escape_chars == 0) {
        memcpy(outbuf, buf, len);
        outbuf[len] = '\0';
        return outbuf;
    }

    ptr = (unsigned char *)buf;

    while (*ptr) {
        if (*ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            __bufput(outbuf, &offset, (char)*ptr);
        } else {
            __bufput(outbuf, &offset, '\\');
            switch (*ptr) {
            case '\\':
                __bufput(outbuf, &offset, '\\');
                break;
            case '\"':
                __bufput(outbuf, &offset, '\"');
                break;
            case '\b':
                __bufput(outbuf, &offset, 'b');
                break;
            case '\n':
                __bufput(outbuf, &offset, 'n');
                break;
            case '\t':
                __bufput(outbuf, &offset, 't');
                break;
            case '\f':
                __bufput(outbuf, &offset, 'f');
                break;
            case '\r':
                __bufput(outbuf, &offset, 'r');
                break;
            default:
                offset += sprintf((char *)outbuf + offset, "u%04x",
                                  (unsigned int)*ptr);
                break;
            }
        }
        ++ptr;
    }
    outbuf[len] = '\0';
    return outbuf;
}

static void printJsonKey(json *J) {
    if (J->key) {
        unsigned char *escape_str = escapeString(J->key);
        printf("\"%s\": ", escape_str);
        free(escape_str);
    }
}

/* print to stdout */
static void __json_print(json *J, int depth) {
    if (J == NULL) {
        return;
    }

    while (J) {
        printDepth(depth);
        switch (J->type) {
        case JSON_INT:
            printJsonKey(J);
            printf("%ld", J->integer);
            break;

        case JSON_FLOAT:
            printJsonKey(J);
            printf("%1.17g", J->floating);
            break;

        case JSON_STRNUM:
            printJsonKey(J);
            printf("%s", J->strnum);
            break;

        case JSON_STRING:
            printJsonKey(J);
            unsigned char *escape_str = escapeString(J->str);
            printf("\"%s\"", escape_str);
            free(escape_str);
            break;

        case JSON_ARRAY: {
            printJsonKey(J);
            json *ptr = J->array;
            printf("[\n");
            __json_print(ptr, depth + 1);
            printDepth(depth);
            printf("]");
            break;
        }

        case JSON_OBJECT:
            printJsonKey(J);
            printf("{\n");
            __json_print(J->object, depth + 1);
            printDepth(depth);
            printf("}");
            break;

        case JSON_BOOL:
            printJsonKey(J);
            if (J->boolean == 1) {
                printf("true");
            } else {
                printf("false");
            }
            break;

        case JSON_NULL:
            printJsonKey(J);
            printf("null");
            break;
        }

        if (J->next) {
            printf(",\n");
        } else {
            printf("\n");
        }

        J = J->next;
    }
}

/**
 * Recursively frees whole JSON object
 */
void jsonRelease(json *J) {
    if (!J) {
        return;
    }
    json *ptr = J;
    json *next = NULL;

    while (ptr) {
        next = ptr->next;
        if (ptr->key) {
            free(ptr->key);
        }

        switch (ptr->type) {

        case JSON_STRNUM:
            free(ptr->strnum);
            break;

        case JSON_STRING:
            free(ptr->str);
            break;

        case JSON_ARRAY:
            jsonRelease(ptr->array);
            break;

        case JSON_OBJECT: {
            jsonRelease(ptr->object);
            break;
        }

        case JSON_FLOAT:
        case JSON_INT:
        case JSON_BOOL:
        case JSON_NULL:
            break;
        }

        free(ptr);
        ptr = next;
    }
}

static char *jsonComposeError(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int bufferlen = 1024; /* Probably big enough */
    char *str_error = malloc(sizeof(char) * bufferlen);

    int actual_len = vsnprintf(str_error, bufferlen, fmt, ap);
    str_error[actual_len] = '\0';

    va_end(ap);
    return str_error;
}

static char *_jsonGetStrerror(JSON_ERRNO error, char ch, size_t offset) {
    switch (error) {
    case JSON_OK:
        return jsonComposeError("No errors");

    case JSON_INVALID_UTF16:
        return jsonComposeError(
                "Unexpected UTF16 character '%c' while parsing UTF16 at position: %zu",
                ch, offset);

    case JSON_INVALID_UTF16_SURROGATE:
        return jsonComposeError(
                "Unexpected UTF16 surrogate character '%c' while parsing UTF16 at position: %zu",
                ch, offset);
    case JSON_INVALID_HEX:
        return jsonComposeError(
                "Unexpected hex '%c' while parsing UTF16 at position: %zu", ch,
                offset);
    case JSON_INVALID_STRING_NOT_TERMINATED:
        return jsonComposeError(
                "Expected '\"' to terminate string recieved '%c' at position: %zu",
                ch, offset);
    case JSON_INVALID_NUMBER:
        return jsonComposeError(
                "Unexpected numeric character '%c' while parsing number at position: %zu",
                ch, offset);
    case JSON_INVALID_DECIMAL:
        return jsonComposeError(
                "Unexpected decimal character '%c' while parsing number at position: %zu",
                ch, offset);
    case JSON_INVALID_SIGN:
        return jsonComposeError(
                "Unexpected sign character '%c' while parsing number at position: %zu",
                ch, offset);

    case JSON_INVALID_BOOL:
        return jsonComposeError(
                "Unexpected character '%c' while parsing boolean at position: %zu",
                ch, offset);
    case JSON_INVALID_JSON_TYPE_CHAR:
    case JSON_INVALID_TYPE:
        return jsonComposeError(
                "Unexpected character '%c' while seeking next type to parse at position: %zu",
                ch, offset);

    case JSON_CANNOT_START_PARSE:
        return jsonComposeError(
                "JSON must start with '[' or '{', at position: %zu", offset);
    case JSON_CANNOT_ADVANCE:
    case JSON_EOF:
        return jsonComposeError(
                "Unexpected end of json buffer at position: %zu", offset);
    }
    return NULL;
}

/* If the json has state this will return the error as a human readible string
 * must be freed by the caller (allows it to be thread safe). It would be
 * advisable to use this for debugging purposes only as it calls malloc */
char *jsonGetStrerror(jsonState *state) {
    if (!state) {
        return NULL;
    }
    return _jsonGetStrerror(state->error, state->ch, state->offset);
}

/* Print the error to stderr */
void jsonPrintError(json *j) {
    char *str_error = jsonGetStrerror(j->state);
    fprintf(stderr, "%s\n", str_error);
    free(str_error);
}

/* Return the global error state */
jsonState *jsonGetGlobalState(void) {
    return &json_global_state;
}

/**
 * Parse null terminated string buffer to a json struct. The length of the json
 * string must be known ahead of time.
 *
 * Pass in flags to modify the behaviour of the parser:
 * - JSON_STRNUM_FLAG: do not try to parse numbers: floats,hex, ints etc..
 *   will be treated as strings.
 * - JSON_STATE_FLAG: Maintain state for the parse, capturing errors
 *
 * You must free the resulting pointer with `jsonRelease`
 */
json *jsonParseWithLenAndFlags(char *raw_json, size_t buflen, int flags) {
    jsonParser p;
    p.flags = flags;
    jsonParserInit(&p, raw_json, buflen);

    jsonAdvanceWhitespace(&p);
    char peek = jsonPeek(&p);
    json *J = jsonNew();

    p.J = J;

    /**
     * Kick off parsing by finding the first non whitespace character,
     * json has to start with either '{' or '['
     */
    if (peek == '{') {
        J->type = JSON_OBJECT;
        J->object = jsonParseObject(&p);
    } else if (peek == '[') {
        J->type = JSON_ARRAY;
        J->array = jsonParseArray(&p);
    } else {
        p.errno = JSON_CANNOT_START_PARSE;
    }

#ifdef ERROR_REPORTING
    if (p.errno != JSON_OK) {
        char *error_buf = _jsonGetStrerror(p.errno, jsonPeek(&p), p.offset);
        json_debug("%s\n", error_buf);
        free(error_buf);
    }
#endif

    /* We only set this if there is an error to save a call to malloc */
    if (p.errno != JSON_OK) {
        /* If the flag has been set we will call malloc */
        if (p.flags & JSON_STATE_FLAG) {
            J->state = jsonStateNew();
            J->state->error = p.errno;
            J->state->ch = jsonPeek(&p);
            J->state->offset = p.offset;
        } else {
            /* Set the global error state */
            json_global_state.ch = jsonPeek(&p);
            json_global_state.error = p.errno;
            json_global_state.offset = p.offset;
        }
    }

    return p.J;
}

/**
 * Parse null terminated string buffer to a json struct. The length of the json
 * string must be known ahead of time.
 *
 * You must free the resulting pointer with `jsonRelease`
 */
json *jsonParseWithLen(char *raw_json, size_t buflen) {
    return jsonParseWithLenAndFlags(raw_json, buflen, JSON_NO_FLAGS);
}

/**
 * Parse null terminated string buffer to a json struct. Strlen will be called
 * to obtain the raw json string's length. Pass in flags to modify the
 * behaviour of the parser.
 *
 * Pass in flags to modify the behaviour of the parser:
 * - JSON_STRNUM_FLAG: do not try to parse numbers: floats,hex, ints etc..
 *   will be treated as strings.
 *
 * You must free the resulting pointer with `jsonRelease`
 */
json *jsonParseWithFlags(char *raw_json, int flags) {
    return jsonParseWithLenAndFlags(raw_json, strlen(raw_json), flags);
}

/**
 * Parse null terminated string buffer to a json struct. Strlen will be called
 * to obtain the raw json string's length.
 *
 * You must free the resulting pointer with `jsonRelease`
 */
json *jsonParse(char *raw_json) {
    return jsonParseWithLen(raw_json, strlen(raw_json));
}

/**
 * Pretty print json to stdout
 */
void jsonPrint(json *J) {
    __json_print(J, 1);
}

int jsonIsObject(json *j) {
    return j && j->type == JSON_OBJECT;
}

int jsonIsArray(json *j) {
    return j && j->type == JSON_ARRAY;
}

int jsonIsNull(json *j) {
    return j && j->type == JSON_NULL;
}

int jsonIsBool(json *j) {
    return j && j->type == JSON_BOOL;
}

int jsonIsString(json *j) {
    return j && j->type == JSON_STRING;
}

int jsonIsInt(json *j) {
    return j && j->type == JSON_INT;
}

int jsonIsFloat(json *j) {
    return j && j->type == JSON_FLOAT;
}

/**
 * Get json string value or NULL
 */
char *jsonGetString(json *J) {
    return J->type == JSON_STRING ? J->str : NULL;
}

/* Get float from json object */
double jsonGetFloat(json *J) {
    return J->type == JSON_FLOAT ? J->floating : 0.0;
}

/* Get int from json object */
ssize_t jsonGetInt(json *J) {
    return J->type == JSON_INT ? J->integer : 0;
}

/* Get string number from json object, will only be present if the json was
 * parsed with JSON_STRNUM_FLAG
 * */
char *jsonGetStrnum(json *J) {
    return J->type == JSON_STRNUM ? J->strnum : NULL;
}

/**
 * Get json array or NULL
 */
json *jsonGetArray(json *J) {
    return J->type == JSON_ARRAY ? J->array : NULL;
}

/**
 * Get json object or NULL
 */
json *jsonGetObject(json *J) {
    return J->type == JSON_OBJECT ? J->object : NULL;
}

/**
 * Get boolean, 1 = true, 0 = false, -1 = error
 */
int jsonGetBool(json *J) {
    return J->type == JSON_BOOL ? J->boolean : -1;
}

/**
 * Get NULL or sentinal value if not NULL
 */
void *jsonGetNull(json *J) {
    return J->type == JSON_NULL ? 0 : JSON_SENTINAL;
}
