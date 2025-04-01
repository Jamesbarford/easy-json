/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef JSON_H
#define JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define JSON_MAX_EXPONENT (511)
#define JSON_SENTINAL     ((void *)(long)0x44)
#define JSON_NO_FLAGS     (0)

/* Do not parse numbers, treat them as strings */
#define JSON_STRNUM_FLAG (1)

typedef enum JSON_DATA_TYPE {
    JSON_STRING,
    JSON_FLOAT,
    JSON_INT,
    JSON_STRNUM,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_BOOL,
    JSON_NULL,
} JSON_DATA_TYPE;

typedef struct jsonState {
    int error;
    char ch;
    size_t offset;
    /* A handle to the memory arena */
    void *mem;
} jsonState;

typedef struct json json;
/* Everything on this struct is created by an arena, do NOT call free on any 
 * of the individual properties */
typedef struct json {
    jsonState *state;
    json *next;
    char *key;
    JSON_DATA_TYPE type;
    union {
        json *array;
        json *object;
        char *str;
        int boolean;
        char *strnum;
        double floating;
        ssize_t integer;
    };
} json;

typedef enum JSON_ERRNO {
    JSON_OK,
    JSON_INVALID_UTF16,
    JSON_INVALID_UTF16_SURROGATE,
    JSON_INVALID_HEX,
    JSON_INVALID_STRING_NOT_TERMINATED,
    JSON_INVALID_NUMBER,
    JSON_INVALID_DECIMAL,
    JSON_INVALID_SIGN,
    JSON_INVALID_JSON_TYPE_CHAR,
    JSON_INVALID_BOOL,
    JSON_INVALID_TYPE,
    JSON_CANNOT_ADVANCE,
    JSON_CANNOT_START_PARSE,
    JSON_INVALID_KEY_TERMINATOR_CHARACTER,
    JSON_INVALID_KEY_VALUE_SEPARATOR,
    JSON_INVALID_ARRAY_CHARACTER,
    JSON_INVALID_ESCAPE_CHARACTER,
    JSON_EOF,
} JSON_ERRNO;

json *jsonGetObject(json *J);
json *jsonGetArray(json *J);
void *jsonGetNull(json *J);
int jsonGetBool(json *J);
char *jsonGetString(json *J);
ssize_t jsonGetInt(json *J);
double jsonGetFloat(json *J);

int jsonIsObject(json *j);
int jsonIsArray(json *j);
int jsonIsNull(json *j);
int jsonIsBool(json *j);
int jsonIsString(json *j);
int jsonIsInt(json *j);
int jsonIsFloat(json *j);

json *jsonParse(char *raw_json);
json *jsonParseWithFlags(char *raw_json, int flags);
json *jsonParseWithLen(char *raw_json, size_t buflen);
json *jsonParseWithLenAndFlags(char *raw_json, size_t buflen, int flags);
void jsonRelease(json *J);

int jsonGetError(json *j);
char *jsonGetStrerror(json *J);
void jsonPrintError(json *J);
char *jsonToString(json *j, size_t *len);
int jsonOk(json *j);
void jsonPrint(json *J);

#ifdef __cplusplus
}
#endif

#endif
