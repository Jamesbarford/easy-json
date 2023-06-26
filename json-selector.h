#ifndef __JSON_SELECTOR_H
#define __JSON_SELECTOR_H

#include "json.h"

json *jsonSelect(json *j, const char *fmt, ...);
json *jsonArrayAt(json *j, int idx);
json *jsonObjectAtCaseSensitive(json *j, const char *name);
json *jsonObjectAtCaseInSensitive(json *j, const char *name);

#endif
