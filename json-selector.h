/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __JSON_SELECTOR_H
#define __JSON_SELECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "json.h"

json *jsonSelect(json *j, const char *fmt, ...);
json *jsonArrayAt(json *j, int idx);
json *jsonObjectAtCaseSensitive(json *j, const char *name);
json *jsonObjectAtCaseInSensitive(json *j, const char *name);

#ifdef __cplusplus
}
#endif

#endif
