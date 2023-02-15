# Easy JSON
Easy JSON is a lightweight and simple to understand JSON parser. With an emphasis on readability and some experimentation with the 'Streaming SIMD Extensions 2' (`__SSE2__`). With the usage of `SIMD` for skipping whitespace it appears to be faster than `cJSON` when used on large JSON files. On small JSON files the speed is pretty much the same.

## Usage
You need `json.c` & `json.h` and you're good.
```c
#include "json.h"

static char json_string = "{\"foo\": \"bar\", \"num\": 42.69}";

int
main(void)
{
    /* Initalise internal structures, needs to be called once */
    jsonInit();

    /* Get JSON */
    json *J = jsonParse(json_string);

    /* Do some things with the JSON */

    /* Free up JSON */
    jsonRelease(J);
}

```

## Working with the code
`main.c` contains a simple program that will read in a file, parse the json, print it and time the `jsonParse` and `jsonRelease` functions.

Compile with the flag `-DERROR_REPORTING` if you want to print errors to
`stderr`. There's a debugging function that can be useful for exploring the code.

I tried a few ideas and have split out the more tricky functions into `parse-string` or `parse-number`. Which allow for seeing how they work without the clutter of the other JSON parsing. `char-bitest` was an expriement creating bitmaps to check for characters, which is very slow in comparison to an `if (ch == ' '` check.

The main structure is very simple and follows the pattern of a linked list.
The conecptual difference between an array and an object in this structure is that an object has a `key` and an array doesn't.
```c
typedef struct json {
    JSON_DATA_TYPE type;
    char *key;
    union {
        char *str;
        long double num;
        int boolean;
        json *object;
        json *array;
    } u;
    json *next;
} json;
```

## How the parsing works
1. Find first character, must be a `{` or a `[`
2. Either call `jsonParseObject` or `jsonParseArray`
3. Find the first character which to be valid must be one of `"` `{` `[` `t` `f` `n` `-` `0..9`
4. Based on that character set the `jsonParser` type
5. Call `jsonParseValue` this will pick the correct function to call and set the value on the `union`
6. Repeat until either out of characters or there is an error which is set on `jsonParser->errno`

The implementations of the parsers in order of least complexity:
- `jsonParseBool`
- `jsonParseNull`
- `jsonParseArray`  - basically moves past whitespace and calls `jsonParseValue`
- `jsonParseObject` - same as array but calls `jsonParseString` to set the `key`
- `jsonParseString` - happy path is simple enough, but parsing `utf-16` makes the code far more tricky.
- `jsonParseNumber` - with a mantissa limit of 18 find both parts of the number as 2 ints and glue it back together

## SIMD
Very limited support for `simd`, currently to `__SSE2__` which was avalible on my macbook pro. It's used for jumping passed whitespace characters.

## Limitations & Future considerations
- Allows duplicate keys, though so does `cJSON`.
- It would be fun to implement the `json` struct as a red black tree, it would be slower to parse JSON but faster to do lookups. It feels unrealistic that you just want to parse JSON usually you want to get at something within the structure and do it quickly.
- Floating point precision is a bit iffy, however the aim was to not `#include <math.h>` which has been achieved.
- I'm sure there is more but this is the first limitation that springs to mind.
