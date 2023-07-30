#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "json.h"

void panic(char *fmt, ...) {
    va_list va;
    char msg[1024];

    va_start(va, fmt);
    vsnprintf(msg, sizeof(msg), fmt, va);
    fprintf(stderr, "%s\n", msg);

    va_end(va);
    exit(EXIT_FAILURE);
}

char *readFile(char *path) {
    char *buffer;
    int fd = open(path, O_RDONLY, 0666);
    if (fd == -1) {
        panic("Path: '%s' does not exist\n");
    }

    long size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if ((buffer = malloc(sizeof(char) * size)) == NULL) {
        panic("Cannot malloc buffer: %s\n", strerror(errno));
    }

    if (read(fd, buffer, size) != size) {
        panic("Failed to read file: %s\n", strerror(errno));
    }

    return buffer;
}

int floatEqual(double a, double b) {
    double max_val = fabsl(a) > fabsl(b) ? fabsl(a) : fabsl(b);
    return (fabsl(a - b) <= max_val * DBL_EPSILON);
}

static int tests = 0, fails = 0, skipped = 0;

#define test(...)                  \
    {                              \
        printf("#%02d ", ++tests); \
        printf(__VA_ARGS__);       \
    }

#define testCondition(passed)                  \
    if (passed)                                \
        printf("\033[0;32mPASSED\033[0;0m\n"); \
    else {                                     \
        printf("\033[0;31mFAILED\033[0;0m\n"); \
        fails++;                               \
    }

void testParsingFloats(void) {
    char *float_1 = "[0.123456789]";
    char *float_2 = "[0.12345]";
    char *float_3 = "[123123123123.123131231231]";
    char *float_4 = "[800800800.7007007007001111]";
    char *float_5 = "[-103.342]";
    char *float_6 = "[12.3.45]"; /* Invalid */
    char *float_7 = "[123.]";
    char *float_8 = "[.123]";
    char *float_9 = "[-1.50139930144708198E18]";

    json *parsed = jsonParse(float_1);
    test("  Parsing %s", float_1);
    testCondition(floatEqual(parsed->array->floating, 0.123456789));
    jsonRelease(parsed);

    parsed = jsonParse(float_2);
    test("  Parsing %s", float_2);
    testCondition(floatEqual(parsed->array->floating, 0.12345));
    jsonRelease(parsed);

    parsed = jsonParse(float_3);
    test("  Parsing %s", float_3);
    testCondition(
            floatEqual(parsed->array->floating, 123123123123.123131231231));
    jsonRelease(parsed);

    parsed = jsonParse(float_4);
    test("  Parsing %s = %1.17g == %1.17Lg", float_4, parsed->array->floating,
         strtold("800800800.7007007007001111", NULL));
    testCondition(
            floatEqual(parsed->array->floating, 800800800.7007007007001111));
    jsonRelease(parsed);

    parsed = jsonParse(float_5);
    test("  Parsing %s", float_5);
    testCondition(floatEqual(parsed->array->floating, -103.342));
    jsonRelease(parsed);

    parsed = jsonParseWithFlags(float_6, JSON_STATE_FLAG);
    test("  Parsing %s", float_6);
    testCondition(parsed->state->error == JSON_INVALID_NUMBER);
    jsonRelease(parsed);

    parsed = jsonParse(float_7);
    test("  Parsing %s", float_7);
    testCondition(floatEqual(parsed->array->floating, 123.0));
    jsonRelease(parsed);

    parsed = jsonParse(float_8);
    test("  Parsing %s", float_8);
    testCondition(floatEqual(parsed->array->floating, 0.123));
    jsonRelease(parsed);

    parsed = jsonParse(float_9);
    test("  Parsing %s", float_9);
    testCondition(floatEqual(parsed->array->floating, -1.50139930144708198E18));
    jsonRelease(parsed);
}

void testParsingInts(void) {
    char *int_1 = "[1234567890]";
    char *int_2 = "[0xFF]";
    char *int_3 = "[-12312312]";
    char *int_4 = "[100]";
    char *int_5 = "[-5514085325291784739]";
    char *int_6 = "[1]";
    char *int_7 = "[0]";
    char *int_8 = "[10101010100101010]";

    json *parsed = jsonParse(int_1);
    test("  Parsing %s", int_1);
    testCondition(parsed->array->integer == 1234567890);
    jsonRelease(parsed);

    parsed = jsonParse(int_2);
    test("  Parsing %s", int_2);
    testCondition(parsed->array->integer == 0xFF);
    jsonRelease(parsed);

    parsed = jsonParse(int_3);
    test("  Parsing %s", int_3);
    testCondition(parsed->array->integer == -12312312);
    jsonRelease(parsed);

    parsed = jsonParse(int_4);
    test("  Parsing %s", int_4);
    testCondition(parsed->array->integer == 100);
    jsonRelease(parsed);

    parsed = jsonParse(int_5);
    test("  Parsing %s", int_5);
    testCondition(parsed->array->integer == -5514085325291784739);
    jsonRelease(parsed);

    parsed = jsonParse(int_6);
    test("  Parsing %s", int_6);
    testCondition(parsed->array->integer == 1);
    jsonRelease(parsed);

    parsed = jsonParse(int_7);
    test("  Parsing %s", int_7);
    testCondition(parsed->array->integer == 0);
    jsonRelease(parsed);

    parsed = jsonParse(int_8);
    test("  Parsing %s", int_8);
    testCondition(parsed->array->integer == 10101010100101010);
    jsonRelease(parsed);
}

void testInvalidJson(void) {
    int invalid_file_id = 0;
    int file_id, len;
    char path[2000], *raw_json;
    json *parsed;
    FILE *fp;
    size_t filelen;

    for (; invalid_file_id < 6; ++invalid_file_id) {
        int file_id = invalid_file_id + 1;
        len = snprintf(path, sizeof(path), "./test-jsons/invalid%d.json",
                       file_id);
        path[len] = '\0';
        raw_json = readFile(path);
        parsed = jsonParse(raw_json);
        // if (parsed && parsed->state->error == JSON_OK) {
        printf("%s\n", raw_json);
        //}
        parsed = NULL;
        free(raw_json);
    }
}

int main(void) {
    printf("Parsing floats\n");
    testParsingFloats();
    printf("Parsing ints\n");
    testParsingInts();
    testInvalidJson();
}
