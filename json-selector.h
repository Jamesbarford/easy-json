#ifndef __JSON_SELECTOR_H
#define __JSON_SELECTOR_H

#include "json.h"
#define JSON_SEL_INVALD (0)
#define JSON_SEL_OBJ (1)
#define JSON_SEL_ARRAY (2)
#define JSON_SEL_TYPECHECK (3)
#define JSON_SEL_MAX_BUF (256)

json *jsonSelect(json *j, const char *fmt, ...);
json *jsonArrayAt(json *j, int idx);
json *jsonObjectAtCaseSensitive(json *j, const char *name);
json *jsonObjectAtCaseInSensitive(json *j, const char *name);

#endif
