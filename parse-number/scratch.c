#include <stdio.h>
#include <string.h>

char ascii_chars[256] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
    92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
    109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
    124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
    139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153,
    154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
    169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
    184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
    199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
    214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
    229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243,
    244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 };

#define CHAR_MAP_SIZE 256
unsigned char char_bmp[CHAR_MAP_SIZE / 8];

/*
unsigned int num_terminators[16] = { 0x00010004, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x0020000 };
*/
unsigned int num_terminators[16] = { 0x41, 0x10, 0x200000, 0x00, 0x0, 0x0, 0x0,
    0x0, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned char num_char_term_bmp[32] = { 0x1, 0x4, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

unsigned int term[16] = {
    0x00000401,
    0x00001000,
    0x20000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
};

static unsigned int char_bmp_dec_numeric[16] = { 0x0000000, 0x03FF0000, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char bmp_dec[32] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x3,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

unsigned int alt[16];

void
init_char_map(void)
{
    memset(char_bmp, 0, CHAR_MAP_SIZE / 8);
    char chars_to_encode[] = { ',', ']', '\0', '\n' };
    int num_chars = sizeof(chars_to_encode) / sizeof(chars_to_encode[0]);

    for (int i = 0; i < num_chars; i++) {
        char c = chars_to_encode[i];
        char_bmp[c / 8] |= 1 << (c % 8);
    }

    unsigned char *foo = (unsigned char *)char_bmp_dec_numeric;
    for (int i = 0; i < 32; ++i) {
        printf("0x%X, ", foo[i]);
    }
    printf("\n");

    for (int i = 0; i < 32; i += 4) {
        unsigned int x = 0;
        x |= num_char_term_bmp[i] | num_char_term_bmp[i + 1] << 8 |
                num_char_term_bmp[i + 2] << 16 | num_char_term_bmp[i + 3] << 24;
        printf("x%08x\n", x);
    }

    for (int i = 0; i < num_chars; ++i) {
        char c = chars_to_encode[i];
        // char c2 = chars_to_encode[i - 1];
        // alt[c2 / 16] |= 1 << (c2 % 16);
        alt[c / 32] |= 1 << (c % 32);
    }

    for (int i = '0'; i <= '9'; i += 4) {
        printf("x%20X%20X%20X%20X\n", 1 << (i % 8), 1 << (i + 1 % 8),
                1 << (i + 2 % 8), 1 << (i + 3 % 8));
    }

    for (int i = 0; i < 16; i++) {
        printf("0x%08X, ", alt[i]);
    }
    printf("\n\n");

    for (int i = 0; i < CHAR_MAP_SIZE / 8; ++i) {
        printf("0x%X, ", char_bmp[i]);
    }
    printf("\n");
}

/* takes a bitmap and a character as input and returns 1 if the character is set
 * in the bitmap, and 0 otherwise. It uses bit shifting and bitwise operations
 * to find the corresponding byte in the bitmap and check if the bit is set.
 */
static inline int
bitTest(unsigned char *bitmap, unsigned char bit_test)
{
    /* Move the pointer to the corresponding byte in the bitmap */
    bitmap += bit_test >> 3;
    /* Calculate bit position within the byte */
    bit_test &= 7;
    return (*bitmap & (1 << bit_test)) ? 1 : 0;
}

int
main(void)
{
    init_char_map();

    printf("unsigned int = %zu\nunsigned char = %zu\n", sizeof(unsigned int),
            sizeof(unsigned char));

    char c = ',';
    if (bitTest(char_bmp, c)) {
        printf("The character '%c' is present in the bitmap.\n", c);
    } else {
        printf("The character '%c' is not present in the bitmap.\n", c);
    }

    c = 'a';
    if (bitTest(char_bmp, c)) {
        printf("The character '%c' is present in the bitmap.\n", c);
    } else {
        printf("The character '%c' is not present in the bitmap.\n", c);
    }

    for (int i = 0; i < sizeof(ascii_chars) / sizeof(ascii_chars[0]); ++i) {
        unsigned char c = ascii_chars[i];
        if (bitTest((unsigned char *)alt, c)) {
            printf("'%c' '%d' is inna de bitmap\n", c, c);
        } else {
            //    printf("'%c' '%d' nah nah bitmap!\n", c, c);
        }
    }

    return 0;
}
