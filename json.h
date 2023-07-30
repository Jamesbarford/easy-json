#ifndef __JSON_H
#define __JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define JSON_MAX_EXPONENT (511)
#define JSON_SENTINAL     ((void *)(long)0x44)
#define JSON_NO_FLAGS     (0)

/* Do not parse numbers, treat them as strings */
#define JSON_STRNUM_FLAG (1)
/* Maintains the state of the parser, will capture errors if there are any */
#define JSON_STATE_FLAG (2)

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
} jsonState;

typedef struct json json;
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
    JSON_OK = 0x0,
    JSON_INVALID_UTF16 = 0x1,
    JSON_INVALID_UTF16_SURROGATE = 0x2,
    JSON_INVALID_HEX = 0x4,
    JSON_INVALID_STRING_NOT_TERMINATED = 0x8,
    JSON_INVALID_NUMBER = 0x10,
    JSON_INVALID_DECIMAL = 0x20,
    JSON_INVALID_SIGN = 0x40,
    JSON_INVALID_JSON_TYPE_CHAR = 0x80,
    JSON_INVALID_BOOL = 0x100,
    JSON_INVALID_TYPE = 0x200,
    JSON_CANNOT_ADVANCE = 0x400,
    JSON_CANNOT_START_PARSE = 0x800,
    JSON_EOF = 0x10000000,
} JSON_ERRNO;

char *jsonGetString(json *J);
double jsonGetFloat(json *J);
json *jsonGetArray(json *J);
json *jsonGetObject(json *J);
int jsonGetBool(json *J);
void *jsonGetNull(json *J);

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
char *jsonGetStrerror(jsonState *state);
void jsonPrintError(json *j);
void jsonPrint(json *J);
void jsonRelease(json *J);

#ifdef __cplusplus
}
#endif

#endif
