/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
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

#include "json-selector.h"
#include "json.h"

json *jsonParseOrPanic(char *raw_json) {
    json *j = jsonParseWithLen(raw_json, strlen(raw_json));
    if (!j) {
        fprintf(stderr, "Json Parser Error - json is NULL\n");
        exit(1);
    } else if (!jsonOk(j)) {
        char *error = jsonGetStrerror(j);
        fprintf(stderr, "Json Parser Error: %s\n",error);
        free(error);
        exit(1);
    }
    return j;
}

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

    if ((buffer = malloc(sizeof(char) * (size + 1))) == NULL) {
        panic("Cannot malloc buffer: %s\n", strerror(errno));
    }

    if (read(fd, buffer, size) != size) {
        panic("Failed to read file: %s\n", strerror(errno));
    }
    buffer[size] = '\0';

    close(fd);
    return buffer;
}

int floatEqual(double a, double b) {
    double max_val = fabsl(a) > fabsl(b) ? fabsl(a) : fabsl(b);
    return (fabsl(a - b) <= max_val * DBL_EPSILON);
}

static int tests = 0, fails = 0;

#define test(...)                  \
    {                              \
        printf("#%02d ", ++tests); \
        printf(__VA_ARGS__);       \
    }

#define testCondition(passed)                   \
    if (passed)                                 \
        printf("  \033[0;32mPASSED\033[0;0m "); \
    else {                                      \
        printf("  \033[0;31mFAILED\033[0;0m "); \
        fails++;                                \
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

    json *parsed = jsonParseOrPanic(float_1);
    testCondition(floatEqual(parsed->array->floating, 0.123456789));
    test("  Parsing %s\n", float_1);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_2);
    testCondition(floatEqual(parsed->array->floating, 0.12345));
    test("  Parsing %s\n", float_2);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_3);
    testCondition(
            floatEqual(parsed->array->floating, 123123123123.123131231231));
    test("  Parsing %s\n", float_3);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_4);
    testCondition(
            floatEqual(parsed->array->floating, 800800800.7007007007001111));
    test("  Parsing %s = %1.17g == %1.17Lg\n", float_4, parsed->array->floating,
         strtold("800800800.7007007007001111", NULL));
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_5);
    testCondition(floatEqual(parsed->array->floating, -103.342));
    test("  Parsing %s\n", float_5);
    jsonRelease(parsed);

    parsed = jsonParse(float_6);
    testCondition(jsonGetError(parsed) == JSON_INVALID_NUMBER);
    test("  Parsing %s\n", float_6);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_7);
    testCondition(floatEqual(parsed->array->floating, 123.0));
    test("  Parsing %s\n", float_7);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_8);
    testCondition(floatEqual(parsed->array->floating, 0.123));
    test("  Parsing %s\n", float_8);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(float_9);
    testCondition(floatEqual(parsed->array->floating, -1.50139930144708198E18));
    test("  Parsing %s\n", float_9);
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

    json *parsed = jsonParseOrPanic(int_1);
    testCondition(parsed->array->integer == 1234567890);
    test("  Parsing %s\n", int_1);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_2);
    testCondition(parsed->array->integer == 0xFF);
    test("  Parsing %s\n", int_2);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_3);
    testCondition(parsed->array->integer == -12312312);
    test("  Parsing %s\n", int_3);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_4);
    testCondition(parsed->array->integer == 100);
    test("  Parsing %s\n", int_4);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_5);
    testCondition(parsed->array->integer == -5514085325291784739);
    test("  Parsing %s\n", int_5);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_6);
    testCondition(parsed->array->integer == 1);
    test("  Parsing %s\n", int_6);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_7);
    testCondition(parsed->array->integer == 0);
    test("  Parsing %s\n", int_7);
    jsonRelease(parsed);

    parsed = jsonParseOrPanic(int_8);
    testCondition(parsed->array->integer == 10101010100101010);
    test("  Parsing %s\n", int_8);
    jsonRelease(parsed);
}

typedef struct invalidJson {
    JSON_ERRNO expected_error;
    char *filepath;
} invalidJson;

void testInvalidJson(void) {
    char *raw_json;
    json *parsed;

    invalidJson test_cases[] = {
            {JSON_INVALID_KEY_TERMINATOR_CHARACTER,
             "./test-jsons/invalid1.json"},
            {JSON_INVALID_KEY_TERMINATOR_CHARACTER,
             "./test-jsons/invalid2.json"},
            {JSON_INVALID_JSON_TYPE_CHAR, "./test-jsons/invalid3.json"},
            {JSON_INVALID_KEY_TERMINATOR_CHARACTER,
             "./test-jsons/invalid4.json"},
            {JSON_INVALID_JSON_TYPE_CHAR, "./test-jsons/invalid5.json"},
            {JSON_INVALID_ESCAPE_CHARACTER, "./test-jsons/invalid6.json"},
    };

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); ++i) {
        invalidJson *test = &test_cases[i];
        raw_json = readFile(test->filepath);
        parsed = jsonParse(raw_json);
        testCondition(parsed->state->error == test->expected_error);
        test("  Parsing %s\n", test->filepath);
        parsed = NULL;
        free(raw_json);
    }
}

void testParseThenToStringAndBack(void) {
    char *raw_json, *jsonstring;
    json *parsed;
    size_t len;
    int ok;

    raw_json = readFile("./test-jsons/sample.json");
    parsed = jsonParse(raw_json);
    testCondition(jsonOk(parsed));
    test("  Parse json\n");

    jsonstring = jsonToString(parsed, &len);
    testCondition(jsonstring != NULL);
    test("  Back to string\n");

    jsonRelease(parsed);
    parsed = jsonParse(jsonstring);
    ok = jsonOk(parsed);
    testCondition(ok);
    if (!ok) {
        jsonPrintError(parsed);
    }
    test("  Parse json string\n");

    jsonRelease(parsed);
    free(jsonstring);
    free(raw_json);
}

int safeStrcmp(char *s1, char *s2) {
    if (s1 == NULL && s2 == NULL) {
        return 1;
    }
    if ((s1 == NULL && s2 != NULL) || (s2 == NULL && s1 != NULL)) {
        return 0;
    }
    return !strcmp(s1, s2);
}

void testJsonSelector(void) {
    char *raw_json = readFile("./test-jsons/massive.json");
    json *j = jsonParseOrPanic(raw_json);
    json *sel = NULL;
    char *expected_emails[] = {
            "Jane Doe",
            "John Smith",
    };
    int arr_len = sizeof(expected_emails) / sizeof(expected_emails[0]);

    if (!jsonOk(j)) {
        jsonPrintError(j);
        panic("Failed to parse JSON\n");
    }

    sel = jsonSelect(j, ".person.name.first");
    testCondition(safeStrcmp(jsonGetString(sel), "John"));
    test("  .person.name.first == \"John\"\n");

    sel = jsonSelect(j, ".person.name.middle.nicknames[1]:s");
    testCondition(safeStrcmp(jsonGetString(sel), "JD"));
    test("  .person.name.middle.nicknames[1]:s == \"JD\"\n");

    sel = jsonSelect(j, ".person.phoneNumbers[*].callHistory[*].direction:s", 2,
                     0);
    testCondition(safeStrcmp(jsonGetString(sel), "incoming"));
    test("  .person.phoneNumbers[*].callHistory[*].direction:s == \"incoming\"\n");

    sel = jsonSelect(j, ".person.bools[3]:b");
    testCondition(sel->boolean == 0);
    test("  .person.bools[3]:b == false\n");

    for (int i = 0; i < arr_len; ++i) {
        sel = jsonSelect(j, ".person.email.mailbox.inbox[*].sender", i);
        testCondition(safeStrcmp(jsonGetString(sel), expected_emails[i]));
        test("  .person.email.mailbox.inbox[*] == %s\n", expected_emails[i]);
    }
    sel = jsonSelect(j, ".person.misc");
    testCondition(jsonIsNull(sel));
    test("  .person.misc == JSON_SENTINAL\n");

    sel = jsonSelect(j, "foo");
    testCondition(sel == NULL);
    test("  invalid selection 1 foo\n");

    sel = jsonSelect(j, ".[12][21][21][21]");
    testCondition(sel == NULL);
    test("  invalid selection 2 .[12][21][21][21]\n");

    sel = jsonSelect(j, ".person.cats");
    testCondition(sel == NULL);
    test("  invalid selection 3 .person.cats\n");
    jsonRelease(j);
}

int main(void) {
    printf("Parsing floats\n");
    testParsingFloats();
    printf("Parsing ints\n");
    testParsingInts();
    printf("Invalid JSON\n");
    testInvalidJson();
    printf("Parse JSON, then to string, then parse the string\n");
    testParseThenToStringAndBack();
    printf("jsonSelect\n");
    testJsonSelector();
}
