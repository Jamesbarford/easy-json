# Easy JSON
JSON parsing simplified.

## Why use this?
* Simple to use.
* No global error handling state.
* High level api jq-like for selecting properties from the parsed JSON.
* Fast. Uses SIMD 2 (for skipping whitespace).
* Differentiate between `ints` and `floats`

Easy JSON is a lightweight and fast JSON parser with an emphasis on ease of use 
for parsing and accessing properties in the parsed JSON. I was wanting something
that had a small surface area, solid error handling and a `jq` like syntax for 
simplifying accessing properties off JSON.

The is error handling and reporting suitable for use with multiple threads as
The error state is unique per parsed JSON. Thus if you are parsing mulitple 
json's in parallel you can pinpoint which instance failed. The offset is 
saved where the parsed failed providing the character and it's offset in the
JSON string.

A speed increase has been obtained by some experimentation with the 'Streaming 
SIMD Extensions 2' (`__SSE2__`). With the usage of `SIMD` for skipping 
whitespace it is in some instances faster than `cJSON` especially when used on 
large JSON files. On small JSON files the speed is much the same.

## Parsing
You need `json.c` & `json.h` and you're good.

```c
/* Get JSON */
json *J = jsonParse(json_string);

/* Do some things with the JSON */
if (!jsonOk(J)) {
    /* Handle error */
}

/* Free up JSON, can handle nulls */
jsonRelease(J);
```

The following functions are avalible for parsing a JSON string:

```c
json *jsonParse(char *raw_json);
json *jsonParseWithFlags(char *raw_json, int flags);
json *jsonParseWithLen(char *raw_json, size_t buflen);
json *jsonParseWithLenAndFlags(char *raw_json, size_t buflen, int flags);
```

### Flags
- `JSON_STRNUM_FLAG` is avalible for parsing both floats and integers as 
  strings.

## Getters & Typechecking
The following will return `1` if the `json *` is not `NULL` and there is a match
on the type:

```c
int jsonIsObject(json *j);
int jsonIsArray(json *j);
int jsonIsNull(json *j);
int jsonIsBool(json *j);
int jsonIsString(json *j);
int jsonIsInt(json *j);
int jsonIsFloat(json *j);
```

And the following for getting a value from a `json *`:

```c
json *jsonGetObject(json *J);
json *jsonGetArray(json *J);
void *jsonGetNull(json *J);
int jsonGetBool(json *J);
char *jsonGetString(json *J);
ssize_t jsonGetInt(json *J);
double jsonGetFloat(json *J);
```

### NULL
A legitimate JSON NULL is parsed to `JSON_SENTINAL` not c's `NULL`. You can use
the helper `jsonIsNull()` to determine this. 

## Accessing properties on the JSON struct
This allows for `jq` like expressions to select properties from json, this makes 
accessing properties on the json object much easier. It is heavily inspired by 
[antirez issue in cJSON](https://github.com/DaveGamble/cJSON/issues/553) with 
adaptations to make it suitable for use with this parser. It is provided as a 
separate file in `json-selector.c` so if you don't require it you are not 
forced to use it.

```json
{
    "foo": "bar",
    "num": 69420,
    "array": [{
        "id": 1,
        "name": "James"
    }]
}
```

Then parse as follows:
```c
/* Get JSON */
json *J = jsonParse(json_string);

/* Access properties on the json */
json *name = jsonSelect(J, ".array[0].name:s");
json *id = jsonSelect(J, ".array[0].id:i");
json *num = jsonSelect(J, ".num:f");

printf("name: %s\n", name->str);
printf("id:   %ld\n", id->integer);
printf("num:  %ld\n", num->floating);

/* Free up JSON */
jsonRelease(J);
```

Allows selections like:

```c
json *J = jsonParse(myjson_string);
json *width = jsonSelect(J,".foo.bar[*].baz",4);
json *height = jsonSelect(J,".tv.type[4].*","name");
json *price = jsonSelect(J,".clothes.shirt[4].price_*", price_type
                         == EUR ? "eur" : "usd");
```
Do not free the returned value, only ever free the whole json structure with
`jsonRelease`.

You can also include a :<type> specifier, typically at the end, to verify the
type of the final JSON object selected. If the type doesn't match, the
function will return `NULL`. For instance, the specifier `.foo.bar:s` will 
return `NULL` unless the root object has a `'foo'` field, which is an object 
with  a `'bar'` field that contains a string. Here is a comprehensive list of
selectors:

`".field"` selects the "field" of the current object.
`"[1234]"` selects the specified index of the current array.
`":<type>"` checks if the currently selected type is of the specified type,
         where the type can be a single letter representing:
- "s" -> string
- "f" -> float
- "i" -> int
- "a" -> array
- "o" -> object
- "b" -> boolean
- "!" -> null

## Error reporting
In order to see where an error occured along with a human readible message can 
be obtained with the following code. 

```c
json *J = jsonParse(json_string);

/* Check to see if the parse worked correctly */
if (!jsonOk(J)) {
    /* Will print the error to stderr */
    jsonPrintError(J);

    /* Get the string for your own purposes, though the method is slow
     * so the checking the J->state->error would be the faster option.
     */
    char *str_error = jsonGetStrerror(J);

     /**
      * Get the error code
      */
    int error_code = jsonGetError(J);

     /* Do something with the error */
    free(str_error);
}

/* Free up JSON */
jsonRelease(J);
```

## Working with the code
`main.c` contains a simple program that will read in a file, parse the json, print it and time the `jsonParse` and `jsonRelease` functions.

Compile with the flag `-DERROR_REPORTING` if you want to print errors to
`stderr`. There's a debugging function that can be useful for exploring the code.

I tried a few ideas and have split out the more tricky functions into `parse-string` or `parse-number`. Which allow for seeing how they work without the clutter of the other JSON parsing. `char-bitest` was an expriement creating bitmaps to check for characters, which is very slow in comparison to an `if (ch == ' '`.

The main structure is very simple and follows the pattern of a linked list.
The conecptual difference between an array and an object in this structure is that an object has a `key` and an array doesn't.
```c
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
- `jsonParseNumber` - with a mantissa limit of 18 find both parts of the number 
   as 2 ints and glue it back together
- `jsonParseString` - happy path is simple enough, but parsing `utf-16` makes 
   the code far more tricky.

## SIMD
Very limited support for `simd`, currently to `__SSE2__` which was avalible on 
my macbook pro. It's used for jumping passed whitespace characters 16 characters 
at a time as opposed to one by one.

## Limitations & Future considerations
- Allows duplicate keys, though so does `cJSON`.
- It would be fun to implement the `json` struct as a red black tree, it would 
  be slower to parse JSON but potentially faster to do lookups. It feels 
  unrealistic that  you just want to parse JSON usually you want to get at
  something within the structure and do it quickly.
- Floating point precision is a bit iffy, however the aim was to not 
  `#include <math.h>` or use `strlod` which has been achieved.
- I'm sure there is more but this is the first limitation that springs to mind.
