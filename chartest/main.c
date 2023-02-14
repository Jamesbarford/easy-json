#include <stddef.h>
#include <stdio.h>
#include <time.h>

static unsigned int char_bmp_white_space[16] = { 0x80002600, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned int char_bmp_dec_numeric[16] = { 0x0000000, 0x03FF0000, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#define ROUNDS (1000000)

#define isWhiteSpace(ch)                                                  \
    (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || \
            ch == '\r')
#define isNum(ch) (ch >= '0' && ch <= '9')

static inline int
bitTest(unsigned char *bitmap, char ch)
{
    char s = ch;
    /* Move the pointer to the corresponding byte in the bitmap */
    bitmap += ch >> 3;
    /* Calculate bit position within the byte */
    ch &= 7;
    return (*bitmap & (1 << ch)) ? 1 : 0;
}

void
testMacro(void)
{
    printf("MACRO\n");
    size_t count = 0;
    clock_t start = clock();
    for (int i = 0; i < ROUNDS; ++i) {
        for (int i = 0; i < 256; ++i) {
            if (isNum(i)) {
                count++;
            }
        }
    }
    clock_t end = clock();
    long double elapsed_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    printf("count = %zu: duration: %Lfms\n", count, elapsed_ms);
}

void
testBitTest(void)
{
    printf("BIT TEST\n");
    size_t count = 0;
    clock_t start = clock();
    for (int i = 0; i < ROUNDS; ++i) {
        for (int i = 0; i < 256; ++i) {
            if (bitTest((unsigned char *)char_bmp_dec_numeric, i)) {
                count++;
            }
        }
    }
    clock_t end = clock();
    long double elapsed_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    printf("count = %zu: duration: %Lfms\n", count, elapsed_ms);
}

int
main(void)
{
    testMacro();
    testBitTest();
}
