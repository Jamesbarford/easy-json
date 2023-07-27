#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json-selector.h"
#include "json.h"

#define JSON_SEL_INVALD    (0)
#define JSON_SEL_OBJ       (1)
#define JSON_SEL_ARRAY     (2)
#define JSON_SEL_TYPECHECK (3)
#define JSON_SEL_MAX_BUF   (256)

/**
 * Get item from an array of json or return null
 */
json *jsonArrayAt(json *j, int idx) {
    if (!j || j->type != JSON_ARRAY) {
        return NULL;
    }

    json *node = j->array;
    int i = 0;

    while (node && i < idx) {
        node = node->next;
        ++i;
    }

    /* The idx was passed the number of elements in the list */
    if (node == NULL && i != idx) {
        return NULL;
    }

    return node;
}

static json *_jsonObjectAt(json *j, const char *name, int insensative) {
    if (j == NULL || name == NULL) {
        return NULL;
    }

    int (*_strcmp)(const char *, const char *);
    json *el = j->object;

    if (insensative) {
        _strcmp = strcasecmp;
    } else {
        _strcmp = strcmp;
    }

    while ((el != NULL) && el->key != NULL && (_strcmp(name, el->key) != 0)) {
        el = el->next;
    }

    if (el == NULL || el->key == NULL) {
        return NULL;
    }

    return el;
}

static int jsonTypeCheck(json *j, char tk) {
    switch (tk) {
    case 's':
        if (!jsonIsString(j))
            return 0;
        return 1;
    case 'n':
        if (!jsonIsNumber(j))
            return 0;
        return 1;
    case 'o':
        if (!jsonIsObject(j))
            return 0;
        return 1;
    case 'a':
        if (!jsonIsArray(j))
            return 0;
        return 1;
    case 'b':
        if (!jsonIsBool(j))
            return 0;
        return 1;
    case '!':
        if (!jsonIsNull(j))
            return 0;
        return 1;
    default:
        return 0;
    }
}

/**
 * Get from an object if the key matches the name - case sensitive
 */
json *jsonObjectAtCaseSensitive(json *j, const char *name) {
    return _jsonObjectAt(j, name, 0);
}

/**
 * Get from an object if the key matches the name - case insensitive
 */
json *jsonObjectAtCaseInSensitive(json *j, const char *name) {
    return _jsonObjectAt(j, name, 1);
}

/**
 * Allows selections like:
 *
 * json *J = jsonParse(myjson_string);
 * json *width = jsonSelect(J,".foo.bar[*].baz",4);
 * json *height = jsonSelect(J,".tv.type[4].*","name");
 * json *price = jsonSelect(J,".clothes.shirt[4].price_*", price_type
 *                          == EUR ? "eur" : "usd");
 *
 * You can also include a :<type> specifier, typically at the end, to verify the
 * type of the final JSON object selected. If the type doesn't match, the
 * function will return NULL. For instance, the specifier .foo.bar:s will return
 * NULL unless the root object has a 'foo' field, which is an object with a
 * 'bar' cfield that contains a string. Here is a comprehensive list of
 * selectors:
 *
 *    ".field" selects the "field" of the current object.
 *    "[1234]" selects the specified index of the current array.
 *    ":<type>" checks if the currently selected type is of the specified type,
 *              where the type can be a single letter representing:
 *      "s" -> string
 *      "n" -> number
 *      "a" -> array
 *      "o" -> object
 *      "b" -> boolean
 *      "!" -> null
 */
json *jsonSelect(json *j, const char *fmt, ...) {
    /**
     * Heavily inspired by:
     * https://github.com/antirez/stonky/blob/main/stonky.c
     *
     * At one point there was a PR in CJSON that rejected this contribution.
     * It is offered here as a separate compilable unit.
     *
     * The fmt string is parsed, the JSON path is built and the
     * appropriate action is performed depending on the type of the selector
     * (Array, Object or Type Check). If a wildcard "*" is found, the next
     * argument from the variable arguments list is used.
     *
     * If any error occurs during the process (for example, the path length
     * exceeds the maximum buffer length, or an invalid type check character is
     * encountered), the function will stop processing and return NULL. If
     * everything goes smoothly, the function will return the selected JSON
     * object.
     */
    int next = JSON_SEL_INVALD;
    char path[JSON_SEL_MAX_BUF + 1], buf[64], *s = NULL;
    int path_len = 0, len = 0;
    va_list ap;

    va_start(ap, fmt);
    const char *ptr = fmt;

    while (1) {
        if (path_len && (*ptr == '\0' || strchr(".[]:", *ptr))) {
            path[path_len] = '\0';
            switch (next) {
            case JSON_SEL_ARRAY:
                j = jsonArrayAt(j, atoi(path));
                if (!j) {
                    goto fail;
                }
                break;
            case JSON_SEL_OBJ:
                j = jsonObjectAtCaseSensitive(j, path);
                if (!j) {
                    goto fail;
                }
                break;
            case JSON_SEL_TYPECHECK: {
                if (!jsonTypeCheck(j, path[0])) {
                    goto fail;
                }
                break;
            }
            case JSON_SEL_INVALD:
                goto fail;
            }
        } else if (next != JSON_SEL_INVALD) {
            if (*ptr != '*') {
                path[path_len] = *ptr++;
                path_len++;
                if (path_len > JSON_SEL_MAX_BUF) {
                    goto fail;
                }
                continue;
            } else {
                if (next == JSON_SEL_ARRAY) {
                    int idx = va_arg(ap, int);
                    len = snprintf(buf, sizeof(buf), "%d", idx);
                    s = buf;
                } else if (next == JSON_SEL_OBJ) {
                    s = va_arg(ap, char *);
                    len = strlen(s);
                } else {
                    goto fail;
                }
                if (path_len + len > JSON_SEL_MAX_BUF) {
                    goto fail;
                }

                memcpy(path + path_len, buf, len);
                path_len += len;
                ptr++;
                continue;
            }
        }

        if (*ptr == ']') {
            ptr++;
        }
        if (*ptr == '\0') {
            break;
        } else if (*ptr == '[') {
            next = JSON_SEL_ARRAY;
        } else if (*ptr == '.') {
            next = JSON_SEL_OBJ;
        } else if (*ptr == ':') {
            next = JSON_SEL_TYPECHECK;
        } else {
            goto fail;
        }
        path_len = 0;
        ptr++;
    }

    va_end(ap);
    return j;

fail:
    va_end(ap);
    return NULL;
}
