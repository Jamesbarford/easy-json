#ifndef __JSON_H
#define __JSON_H

#include <stddef.h>

#define JSON_MAX_EXPONENT (511)
#define JSON_SENTINAL ((void *)(long)0x44)

typedef enum JSON_DATA_TYPE {
    JSON_STRING,
    JSON_NUMBER,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_BOOL,
    JSON_NULL,
} JSON_DATA_TYPE;

typedef struct json json;
typedef struct json {
    JSON_DATA_TYPE type;
    char *key;
    union {
        char *str;
        double num;
        int boolean;
        json *object;
        json *array;
    } u;
    json *next;
} json;

enum JSON_ERRNO {
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
    JSON_EOF,
};

char *jsonGetString(json *J);
double jsonGetNumber(json *J);
json *jsonGetArray(json *J);
json *jsonGetObject(json *J);
int jsonGetBool(json *J);
void *jsonGetNull(json *J);

int jsonIsObject(json *j);
int jsonIsArray(json *j);
int jsonIsNull(json *j);
int jsonIsNumber(json *j);
int jsonIsBool(json *j);
int jsonIsString(json *j);

json *jsonParse(char *buffer);
json *jsonParseWithLen(char *buffer, size_t buflen);
void jsonPrint(json *J);
void jsonRelease(json *J);
void jsonInit(void);

#endif
