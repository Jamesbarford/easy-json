#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __isdigit(x) ((x) >= '0' && (x) <= '9')
#define static_size(x) (sizeof((x)) / sizeof((x)[0]))

#define debug(...)                                                         \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

#define JSON_MAX_EXPONENT (511)
#define STAMP debug("stamp\n")

/* bitmaps that represents the character sets, where each bit represents a
 * character. The first 16 bits represent characters 0 to 15, and so on. If a
 * bit is set, it means the corresponding character is within the set
 */
static unsigned int char_bmp_white_space[16] = { 0x80002600, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * x1
    x2
    x4
    x8
    x10
    x20
    x40
    x80
    x1
    x2
 *
 */

static unsigned int char_bmp_dec_numeric[16] = { 0x0000000, 0x03FF0000, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned int char_bmp_hex_numeric[16] = { 0x0000000, 0x03FF0000, 0x7E,
    0x7E, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned int char_bmp_num_terminators[16] = { 0x00000401, 0x00001000,
    0x20000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };

static double cachePow10[308 + 308 + 1];

static double powersOf10[] = {
    10.,
    100.,
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256,
};

static inline char
toUpper(char c)
{
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

static inline int
isHex(char c)
{
    char u = toUpper(c);
    return u >= 'A' && u <= 'Z';
}

static inline int
toInt(char c)
{
    return c - '0';
}

static inline int
toHex(char c)
{
    return toUpper(c) - 'A' + 10;
}

static inline double
F64Pow10(int idx)
{
    return powersOf10[idx];
}

static inline double
I64Pow10(int idx)
{
    if (idx > 308) {
        return idx;
    } else if (idx < -308) {
        return 0.0;
    }
    printf("idx:%d %f\n%f\n", idx+309, cachePow10[idx], cachePow10[idx + 309]);
    return cachePow10[idx + 309];
}

/* takes a bitmap and a character as input and returns 1 if the character is set
 * in the bitmap, and 0 otherwise. It uses bit shifting and bitwise operations
 * to find the corresponding byte in the bitmap and check if the bit is set.
 */
int
bitTest(unsigned char *bitmap, char ch)
{
    char s = ch;
    /* Move the pointer to the corresponding byte in the bitmap */
    bitmap += ch >> 3;
    /* Calculate bit position within the byte */
    ch &= 7;
    return (*bitmap & (1 << ch)) ? 1 : 0;
}
#define Bt(bmp, ch) (bitTest((unsigned char *)(bmp), (ch)))

static inline int
doubleEQ(double a, double b)
{

    double max_val = fabsl(a) > fabsl(b) ? fabsl(a) : fabsl(b);
    return (fabsl(a - b) <= max_val * DBL_EPSILON);
}

typedef struct jsonParser {
    char *buffer;
    size_t offset;
    size_t buflen;
    int errno;
} jsonParser;

static inline char
jsonUnsafePeekAt(jsonParser *p, size_t idx)
{
    return p->buffer[idx];
}

static inline char
jsonPeek(jsonParser *p)
{
    return p->buffer[p->offset];
}

static inline int
jsonCanAdvanceBy(jsonParser *p, size_t jmp)
{
    return p->offset + jmp < p->buflen;
}

static inline void
jsonUnsafeAdvanceBy(jsonParser *p, size_t jmp)
{
    p->offset += jmp;
}

static inline void
jsonAdvance(jsonParser *p)
{
    if (jsonCanAdvanceBy(p, 1)) {
        ++p->offset;
        return;
    }
    debug("SET EOF\n");
    debug("[%zu]: '%c' %d\n", p->offset, p->buffer[p->offset],
            p->buffer[p->offset]);
    p->errno = -1;
}

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
        char c = *ptr;
        /* If the character is not a decimal numeric character, or if
         * the decimal point has already been found exit the loop
         */
        if (!Bt(char_bmp_dec_numeric, c)) {
            if ((c != '.') || *decidx >= 0) {
                break;
            }
            *decidx = mantissa;
        }
        ptr++;
    }
    return mantissa;
}

static char *
stringifyNumber(double num)
{
    char *buf = NULL;
    size_t len = 0;
    char tmp[26] = { 0 };
    double test = 0.0;

    len = sprintf(tmp, "%1.15g", num);

    if (sscanf(tmp, "%lf", &test) != 1 || !doubleEQ(test, num)) {
        len = sprintf(tmp, "%1.17g", num);
    }

    if (len < 0 || len > sizeof(tmp)) {
        return NULL;
    }

    buf = malloc(sizeof(char) * len);
    memcpy(buf, tmp, len);
    buf[len] = '\0';

    return buf;
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

    while (!Bt(char_bmp_num_terminators, *ptr)) {
        debug("'%c'\n", *ptr);
        switch (*ptr) {
        case 'e':
        case 'E':
            if (seen_e) {
                printf("asa\n");
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
            if (!Bt(char_bmp_hex_numeric, *ptr)) {
                debug("NON hex: '%c'\n", *ptr);
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

static inline long
stringToHex(jsonParser *p)
{
    long retval = 0;

    while (!Bt(char_bmp_num_terminators, jsonPeek(p))) {
        char ch = jsonPeek(p);
        if (Bt(char_bmp_dec_numeric, ch)) {
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
    unsigned long retval = 0;
    int neg = 0;
    int radix = 10;
    char cur;
    int run = 1;
    int exponent = 0;
    int exponent_sign = 0;

    /* ADVANCE WHITESPACE */
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
            p->errno = -1;
            return 0;
        }
    }

    run = 1;
    while (run) {
        cur = jsonPeek(p);
        switch (cur) {
        case 'e':
        case 'E':
            run = 0;
            continue;
        case ',':
        case '\n':
        case ']':
        case '\0':
            run = 0;
            goto out;
            break;
        default:
            retval = retval * 10 + toInt(cur);
            break;
        }
        jsonUnsafeAdvanceBy(p, 1);
    }

    if (toUpper(jsonPeek(p)) == 'E') {
        jsonUnsafeAdvanceBy(p, 1);
        if (jsonPeek(p) == '-') {
            exponent_sign = 1;
            jsonUnsafeAdvanceBy(p, 1);
        } else if (jsonPeek(p) == '+') {
            jsonUnsafeAdvanceBy(p, 1);
        }
        while (Bt(char_bmp_dec_numeric, jsonPeek(p)) &&
                exponent < INT_MAX / 100) {
            exponent = exponent * 10 + toInt(jsonPeek(p));
            jsonUnsafeAdvanceBy(p, 1);
        }
        while (Bt(char_bmp_dec_numeric, jsonPeek(p))) {
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
    int num_len = countNumberLen(p->buffer);
    if (num_len == 0) {
        return 0;
    }

    char *ptr = p->buffer;
    char *exponent_ptr = NULL;
    size_t num = 0;
    size_t frac = 0;
    size_t div = 1;
    int exponent = 0;
    int fraction_exponent = 0;
    int exponent_sign = 0;
    int dropped = 0;
    int dec_idx = -1;
    int neg = 0;

    while (Bt(char_bmp_white_space, *ptr)) {
        ptr++;
    }

    if (*ptr == '-') {
        neg = 1;
        ptr++;
    } else if (*ptr == '+') {
        neg = 0;
        ptr++;
    } else if (!Bt(char_bmp_dec_numeric, *ptr) && *ptr != '.') {
        jsonCanAdvanceBy(p, ptr - p->buffer);
        return 0.0;
    }

    int mantissa = countMantissa(ptr, &dec_idx);

    exponent_ptr = ptr + mantissa;

    if (dec_idx == -1) {
        debug("StringToI64\n");
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

    debug("fraction_exponent: %d\n", fraction_exponent);

    if (mantissa == 0) {
        debug("no mantissa\n");
        jsonUnsafeAdvanceBy(p, num_len);
        return 0.0;
    }

    debug("mantissa: %d\n", mantissa);
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
            printf("minus E\n");
            exponent_sign = -1;
            ptr++;
        } else if (*ptr == '+') {
            ptr++;
        }
        while (Bt(char_bmp_dec_numeric, *ptr) && exponent < INT_MAX / 100) {
            exponent = exponent * 10 + (*ptr - '0');
            ptr++;
        }
        while (Bt(char_bmp_dec_numeric, *ptr)) {
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
    double __retval = fraction;
    if (exponent_sign) {
        __retval /= double_exponent;
    } else {
        __retval *= double_exponent;
    }

    printf("------\n");
    debug("exp     : %d\n", exponent);
    debug("dub_exp : %f\n", double_exponent);
    debug("ii      : %d\n", ii);
    debug("jj      : %d\n", jj);
    debug("Fraction: %f\n", fraction);
    debug("FLOAT   : %g\n", __retval);
    printf("------\n");

    printf("NUM LEN: %ld\n", ptr - p->buffer);
    jsonUnsafeAdvanceBy(p, num_len);
    if (neg) {
        return -__retval;
    }
    return __retval;
}

jsonParser *
loadNumber(jsonParser *p, char *buf)
{
    p->buffer = buf;
    p->buflen = strlen(buf);
    p->errno = 0;
    p->offset = 0;
    return p;
}

static inline double
power10(double exponent)
{
    double result = 1;
    for (int i = 0; i < exponent; i++) {
        result *= 10;
    }
    return result;
}

int
main(void)
{
    char *qs[] = {
        "-5514085325291784739",
        "100",
        "800800800.7007007007001111",
        "1e10",
        "-103.342",
        "6.0221409e+23",
        "1.23E-5",
        "3.14e5",
        "12.3.45",
        "1.23e+45e-12",
        "123.",
        ".123",
        "0b1010",
        "0o777",
        "0x7F",
        "-1.50139930144708198E18",
        "5.694374481577558E-20",
        "5.69437448157756e-20",
    };
    jsonParser p;

    for (int i = -308; i < 309; i++) {
        cachePow10[i + 309] = power10(i);
    }

    for (int i = 0; i < sizeof(qs) / sizeof(qs[0]); ++i) {
        char *test = qs[i];
        double mine = jsonParseNumber(loadNumber(&p, test));
        double expected = strtold(test, NULL);
        char *minestr = stringifyNumber(mine);
        char *expectedstr = stringifyNumber(expected);

        printf("strlen: %zu\n", strlen(test));
        printf("Parsing: [%s]\nmine: %.30g, expected: %.30g EQ: %s\nmine: %s, expected: %s\n---\n",
                test, mine, expected,
                doubleEQ(mine, expected) ? "true" : "false ", minestr,
                expectedstr);
    }

    /*
    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num1,
            jsonParseNumber(loadNumber(&p, num1)), strtold(num1, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num2,
            jsonParseNumber(loadNumber(&p, num2)), strtold(num2, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num3,
            jsonParseNumber(loadNumber(&p, num3)), strtold(num3, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num4,
            jsonParseNumber(loadNumber(&p, num4)), strtold(num4, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num5,
            jsonParseNumber(loadNumber(&p, num5)), strtold(num5, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", num6,
            jsonParseNumber(loadNumber(&p, num6)), strtold(num6, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", multiple,
            jsonParseNumber(loadNumber(&p, multiple)), strtold(multiple, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", exp,
            jsonParseNumber(loadNumber(&p, exp)), strtold(exp, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", trail,
            jsonParseNumber(loadNumber(&p, trail)), strtold(trail, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", lead,
            jsonParseNumber(loadNumber(&p, lead)), strtold(lead, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", bin,
            jsonParseNumber(loadNumber(&p, bin)), strtold(bin, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", oct,
            jsonParseNumber(loadNumber(&p, oct)), strtold(oct, NULL));

    printf("Parsing: [%s]\nmine: %Lg, expected: %Lg\n---\n", hex,
            jsonParseNumber(loadNumber(&p, hex)), strtold(hex, NULL));
    */
}
