#include <limits.h>
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
#define toInt(ch) (ch - '0')
#define toUpper(ch) ((ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch)
#define toHex(ch) (toUpper(ch) - 'A' + 10)
#define isNumTerminator(ch) (ch == ',' || ch == ']' || ch == '\0' || ch == '\n')
#define numStart(ch) (isNum(ch) || ch == '-' || ch == '+' || ch == '.')

#define debug(...)                                                         \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

#define debug_panic(...)                                                   \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
        exit(EXIT_FAILURE);                                                \
    } while (0)

typedef struct jsonParser {
    /* data type being parsed */
    JSON_DATA_TYPE type;
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
} jsonParser;

/* Pre-calculate powers of 10 */
static double cache_pow10[308 + 308 + 1];

/* Flag to determine if we need to initialise the cached powers of 10 */
static int has_initalised_powers = 0;

/* Retrun power of 10 from the cache */
static double
I64Pow10(int idx)
{
    if (idx > 308) {
        return idx;
    } else if (idx < -308) {
        return 0.0;
    }
    return cache_pow10[idx + 309];
}

/**
 * Use SMID if avalible for machine, this is the one I have on my
 * computer hence the one I've implemented. Makes moving past
 * whitespace faster.
 */
#if defined(__SSE2__)
#include <emmintrin.h>
static size_t
getNextNonWhitespaceIdx(const char *ptr)
{
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
        { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                ' ', ' ' },
        { '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
                '\n', '\n', '\n', '\n', '\n' },
        { '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
                '\r', '\r', '\r', '\r', '\r' },
        { '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
                '\t', '\t', '\t', '\t', '\t' },
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
static size_t
getNextNonWhitespaceIdx(const char *ptr)
{
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
static json *
jsonNew(void)
{
    json *J = malloc(sizeof(json));
    J->key = NULL;
    J->type = JSON_NULL;
    J->next = NULL;
    return J;
}

/**
 * Needs to be null checked before calling this function
 */
static char
jsonUnsafePeekAt(jsonParser *p, size_t idx)
{
    return p->buffer[idx];
}

/**
 * Look at the current character
 */
static char
jsonPeek(jsonParser *p)
{
    return p->buffer[p->offset];
}

/**
 * Check if we can advance the buffer by 'jmp' characters
 */
static int
jsonCanAdvanceBy(jsonParser *p, size_t jmp)
{
    return p->offset + jmp < p->buflen;
}

/**
 * Advance the offset into the buffer by 'jmp'
 */
static void
jsonUnsafeAdvanceBy(jsonParser *p, size_t jmp)
{
    p->offset += jmp;
}

/**
 * Advance buffers offset
 */
static void
jsonAdvance(jsonParser *p)
{
    if (jsonCanAdvanceBy(p, 1)) {
        ++p->offset;
        return;
    }
    p->errno = JSON_EOF;
}

/**
 * Advance to termintor
 */
static void
jsonAdvanceToTerminator(jsonParser *p, char terminator)
{
    while (jsonCanAdvanceBy(p, 1) && jsonPeek(p) != terminator) {
        ++p->offset;
    }
}

/**
 * Advance past whitespace characters
 */
static void
jsonAdvanceWhitespace(jsonParser *p)
{
    p->offset += getNextNonWhitespaceIdx(p->buffer + p->offset);
}

/**
 * Json parser ready to rock and roll
 */
static void
jsonParserInit(jsonParser *p, char *buffer, size_t buflen)
{
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
static double jsonParseNumber(jsonParser *p);

/* Number parsing */

/* Takes a string pointer and a pointer to an integer as input, and returns the
 * number of decimal numeric characters in the string, and the position of the
 * decimal point if present.
 */
static int
countMantissa(char *ptr, int *decidx)
{
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
static int
countNumberLen(char *ptr)
{
    char *start = ptr;
    int seen_e = 0;
    int seen_dec = 0;
    int seen_X = 0;

    while (!isNumTerminator(*ptr)) {
        switch (*ptr) {
        case 'e':
        case 'E':
            if (seen_e) {
                return 0;
            }
            seen_e = 1;
            break;
        case 'x':
        case 'X':
            if (seen_X) {
                return 0;
            }
            seen_X = 1;
            break;
        case '-':
        case '+':
            break;

        case '.':
            if (seen_dec) {
                return 0;
            }
            seen_dec = 1;
            break;
        /* Anything else is invalid */
        default:
            if (!isHex(*ptr)) {
                return 0;
            }
            break;
        }
        ptr++;
    }

    /* Floating point hex does not exist */
    if (seen_dec && seen_X) {
        return 0;
    }

    /* Exponent hex does not exist */
    if (seen_X && seen_e) {
        return 0;
    }

    return ptr - start;
}

static long
stringToHex(jsonParser *p)
{
    long retval = 0;

    while (!isNumTerminator(jsonPeek(p))) {
        char ch = jsonPeek(p);
        if (isNum(ch)) {
            retval = retval * 16 + toInt(ch);
        } else if (isHex(ch)) {
            retval = retval * 16 + toHex(ch);
        } else {
            p->errno = -1;
            break;
        }
        jsonUnsafeAdvanceBy(p, 1);
    }

    return retval;
}

static long
stringToI64(jsonParser *p)
{
    long retval = 0;
    int neg = 0;
    char cur;
    int exponent = 0;
    int exponent_sign = 0;

    if (jsonPeek(p) == '-' || jsonPeek(p) == '+') {
        neg = 1;
        jsonUnsafeAdvanceBy(p, 1);
    }

    if (jsonPeek(p) == '0') {
        jsonUnsafeAdvanceBy(p, 1);
        if (toUpper(jsonPeek(p)) == 'X') {
            jsonUnsafeAdvanceBy(p, 1);
            return stringToHex(p);
        } else {
            p->errno = JSON_INVALID_NUMBER;
            return 0;
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

/**
 * Parse string to a double
 */
static double
jsonParseNumber(jsonParser *p)
{
    char *ptr = p->buffer + p->offset;
    int num_len = countNumberLen(ptr);

    if (num_len == 0) {
        p->errno = JSON_INVALID_NUMBER;
        return 0;
    } else if (num_len == 1) {
        double retval = toInt(jsonPeek(p));
        jsonAdvance(p);
        return retval;
    }

    int exponent = 0;
    int fraction_exponent = 0;
    int exponent_sign = 0;
    int dec_idx = -1;
    int neg = 0;

    if (*ptr == '-') {
        neg = 1;
        ptr++;
    } else if (*ptr == '+') {
        neg = 0;
        ptr++;
    } else if (!isNum(*ptr) && *ptr != '.') {
        p->errno = JSON_INVALID_NUMBER;
        return 0.0;
    }

    int mantissa = countMantissa(ptr, &dec_idx);

    if (dec_idx == -1) {
        // jsonUnsafeAdvanceBy(p, ptr - p->buffer);
        return stringToI64(p);
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

    if (mantissa == 0) {
        jsonUnsafeAdvanceBy(p, num_len);
        return 0.0;
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

/**
 * Convert first 4 characters of buf to a decimal
 * representation
 */
static unsigned int
parseHex4(const unsigned char *buf)
{
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
static void
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

static unsigned int
jsonParseUTF16(jsonParser *p)
{
    if (!jsonCanAdvanceBy(p, 5)) {
        p->errno = JSON_CANNOT_ADVANCE;
        return 0;
    }
    unsigned int codepoint = parseHex4(
            (const unsigned char *)p->buffer + p->offset + 1);

    if (codepoint == INT_MAX) {
        p->errno = JSON_INVALID_HEX;
        return 0;
    }

    /* 5 -> 'u1111' is 5 chars */
    jsonUnsafeAdvanceBy(p, 4);

    /* UTF-16 pair */
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        if (codepoint <= 0xDBFF) {
            if (!jsonCanAdvanceBy(p, 6)) {
                p->errno = JSON_CANNOT_ADVANCE;
                return 0;
            }

            jsonAdvance(p);
            if (jsonPeek(p) != '\\' &&
                    jsonUnsafePeekAt(p, p->offset + 1) != 'u') {
                p->errno = JSON_INVALID_UTF16;
                return 0;
            }

            jsonUnsafeAdvanceBy(p, 2);

            unsigned int codepoint2 = parseHex4(
                    (const unsigned char *)p->buffer + p->offset);
            if (codepoint2 == INT_MAX) {
                p->errno = JSON_INVALID_HEX;
                return 0;
            }

            /* Invalid surrogate */
            if (codepoint2 < 0xDC00 || codepoint2 > 0xDFFF) {
                p->errno = JSON_INVALID_UTF16_SURROGATE;
                return 0;
            }
            codepoint = 0x10000 +
                    (((codepoint & 0x3FF) << 10) | (codepoint2 & 0x3FF));

            /*
            codepoint = (((codepoint - 0xD800) << 10) | (codepoint2 - 0xDC00)) +
                    0x10000;
            */
            jsonCanAdvanceBy(p, 3);
        } else {
            p->errno = JSON_INVALID_UTF16;
            return 0;
        }
    }

    return codepoint;
}

static char *
jsonParseString(jsonParser *p)
{
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
static int
jsonParseBool(jsonParser *p)
{
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
        p->errno = JSON_INVALID_BOOL;
    }
    return retval;
}

/**
 * Parse json null
 * 1: success
 * -1: error
 */
static int
jsonParseNull(jsonParser *p)
{
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
static int
jsonSetExpectedType(jsonParser *p)
{
    char peek = jsonPeek(p);

    switch (peek) {
    case '{':
        p->type = JSON_OBJECT;
        break;

    case '[':
        p->type = JSON_ARRAY;
        break;

    case 'n':
        p->type = JSON_NULL;
        break;

    case 't':
    case 'f':
        p->type = JSON_BOOL;
        break;

    case '"':
        p->type = JSON_STRING;
        break;

    default: {
        if (numStart(peek)) {
            p->type = JSON_NUMBER;
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
static json *
jsonParseObject(jsonParser *p)
{
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
static json *
jsonParseArray(jsonParser *p)
{
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
static int
jsonParseValue(jsonParser *p)
{
    json *J = p->ptr;

    switch (p->type) {
    case JSON_NUMBER:
        J->type = JSON_NUMBER;
        J->num = jsonParseNumber(p);
        break;

    case JSON_STRING:
        J->type = JSON_STRING;
        J->str = jsonParseString(p);
        break;

    case JSON_NULL:
        if (jsonParseNull(p)) {
            J->type = JSON_NULL;
        }
        break;

    case JSON_OBJECT:
        J->type = JSON_OBJECT;
        J->object = jsonParseObject(p);
        break;

    case JSON_BOOL:
        J->type = JSON_BOOL;
        J->boolean = jsonParseBool(p);
        break;

    case JSON_ARRAY:
        J->type = JSON_ARRAY;
        J->array = jsonParseArray(p);
        break;
    }

    return p->errno == JSON_OK;
}

/**
 * Kick off parsing by finding the first non whitespace character,
 * json has to start with either '{' or '['
 */
static int
__jsonParse(jsonParser *p)
{
    jsonAdvanceWhitespace(p);
    char peek = jsonPeek(p);
    json *J = jsonNew();
    p->J = J;

    if (peek == '{') {
        J->type = JSON_OBJECT;
        J->object = jsonParseObject(p);
    } else if (peek == '[') {
        J->type = JSON_ARRAY;
        J->array = jsonParseArray(p);
    } else {
        p->errno = JSON_CANNOT_START_PARSE;
        return 0;
    }

    return p->errno == JSON_OK;
}

static void
printDepth(int depth)
{
    for (int i = 0; i < depth - 1; ++i) {
        printf("  ");
    }
}

static unsigned char *
escapeString(char *buf)
{
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

static void
printNumber(json *J)
{
    size_t len = 0;
    char tmp[26] = { 0 };
    double num = J->num;
    double test = 0.0;

    len = sprintf(tmp, "%1.15g", num);

    if (sscanf(tmp, "%lg", &test) != 1) {
        len = sprintf(tmp, "%1.17g", num);
    }

    if (len < 0 || len > sizeof(tmp) + 1) {
        return;
    }
    tmp[len] = '\0';
    printf("%s", tmp);
}

static void
printJsonKey(json *J)
{
    if (J->key) {
        unsigned char *escape_str = escapeString(J->key);
        printf("\"%s\": ", escape_str);
        free(escape_str);
    }
}

/* print to stdout */
static void
__json_print(json *J, int depth)
{
    if (J == NULL) {
        return;
    }

    while (J) {
        printDepth(depth);
        switch (J->type) {
        case JSON_NUMBER:
            printJsonKey(J);
            printNumber(J);
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
void
jsonRelease(json *J)
{
    json *ptr = J;
    json *next = NULL;

    while (ptr) {
        next = ptr->next;
        if (ptr->key) {
            free(ptr->key);
        }

        switch (ptr->type) {
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

        case JSON_NUMBER:
        case JSON_BOOL:
        case JSON_NULL:
            break;
        }

        free(ptr);
        ptr = next;
    }
}

#ifdef ERROR_REPORTING
static void
jsonPrintError(jsonParser *p)
{
    char c = jsonPeek(p);
    size_t offset = p->offset;

    switch (p->errno) {
    case JSON_OK:
        /* NOT REACHED */
        break;
    case JSON_INVALID_UTF16:
        debug("Unexpected UTF16 character '%c' while parsing UTF16 at position: %zu\n",
                c, offset);
        break;
    case JSON_INVALID_UTF16_SURROGATE:
        debug("Unexpected UTF16 surrogate character '%c' while parsing UTF16 at position: %zu\n",
                c, offset);
        break;
    case JSON_INVALID_HEX:
        debug("Unexpected hex '%c' while parsing UTF16 at position: %zu\n", c,
                offset);
        break;
    case JSON_INVALID_STRING_NOT_TERMINATED:
        debug("Expected '\"' to terminate string recieved '%c' at position: %zu",
                c, offset);
        break;
    case JSON_INVALID_NUMBER:
        debug("Unexpected numeric character '%c' while parsing number at position: %zu\n",
                c, offset);
        break;
    case JSON_INVALID_DECIMAL:
        debug("Unexpected decimal character '%c' while parsing number at position: %zu\n",
                c, offset);
        break;
    case JSON_INVALID_SIGN:
        debug("Unexpected sign character '%c' while parsing number at position: %zu\n",
                c, offset);
        break;

    case JSON_INVALID_BOOL:
        debug("Unexpected character '%c' while parsing boolean at position: %zu\n",
                c, offset);
        break;
    case JSON_INVALID_JSON_TYPE_CHAR:
    case JSON_INVALID_TYPE:
        debug("Unexpected character '%c' while seeking next type to parse at position: %zu\n",
                c, offset);
        break;

    case JSON_CANNOT_START_PARSE:
        debug("JSON must start with '[' or '{', at position: %zu\n", offset);
        break;
    case JSON_CANNOT_ADVANCE:
    case JSON_EOF:
        debug("Unexpected end of json buffer at position: %zu\n", offset);
        break;
    }
}
#else
static void
jsonPrintError(jsonParser *p)
{
    return;
}
#endif

/**
 * Parse null terminated string buffer to a json struct
 * caller free's buffer, must pass in the length of the
 * buffer.
 */
json *
jsonParseWithLen(char *buffer, size_t buflen)
{
    jsonParser p;
    jsonParserInit(&p, buffer, buflen);
    __jsonParse(&p);
    if (p.errno != JSON_OK) {
        jsonPrintError(&p);
        jsonRelease(p.J);
        return NULL;
    }

    return p.J;
}

/**
 * Parse null terminated string buffer to a json struct
 * caller free's buffer will call `strlen` to get the
 * length of the input buffer.
 */
json *
jsonParse(char *buffer)
{
    return jsonParseWithLen(buffer, strlen(buffer));
}

/**
 * Prep cached powers of 10, needs to be called once for
 * an applications lifetime
 */
void
jsonInit(void)
{
    if (!has_initalised_powers) {
        for (int exponent = -308; exponent < 309; exponent++) {
            double result = 1;
            for (int i = 0; i < exponent; i++) {
                result *= 10;
            }
            cache_pow10[exponent + 309] = result;
        }
        has_initalised_powers = 1;
    }
}

/**
 * Pretty print json to stdout
 */
void
jsonPrint(json *J)
{
    __json_print(J, 1);
}

int
jsonIsObject(json *j)
{
    return j && j->type == JSON_OBJECT;
}

int
jsonIsArray(json *j)
{
    return j && j->type == JSON_ARRAY;
}

int
jsonIsNull(json *j)
{
    return j && j->type == JSON_NULL;
}

int
jsonIsNumber(json *j)
{
    return j && j->type == JSON_NUMBER;
}

int
jsonIsBool(json *j)
{
    return j && j->type == JSON_BOOL;
}

int
jsonIsString(json *j)
{
    return j && j->type == JSON_STRING;
}

/**
 * Get json string value or NULL
 */
char *
jsonGetString(json *J)
{
    return J->type == JSON_STRING ? J->str : NULL;
}

/**
 * FIXME: cannot return -1
 * Get double value or -1
 */
double
jsonGetNumber(json *J)
{
    return J->type == JSON_NUMBER ? J->num : -1;
}

/**
 * Get json array or NULL
 */
json *
jsonGetArray(json *J)
{
    return J->type == JSON_ARRAY ? J->array : NULL;
}

/**
 * Get json object or NULL
 */
json *
jsonGetObject(json *J)
{
    return J->type == JSON_OBJECT ? J->object : NULL;
}

/**
 * Get boolean, 1 = true, 0 = false, -1 = error
 */
int
jsonGetBool(json *J)
{
    return J->type == JSON_BOOL ? J->boolean : -1;
}

/**
 * Get NULL or sentinal value if not NULL
 */
void *
jsonGetNull(json *J)
{
    return J->type == JSON_NULL ? 0 : JSON_SENTINAL;
}
