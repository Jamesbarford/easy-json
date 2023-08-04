/**
 * Command line wrapper for Easy JSON, times parsing a json buffer and times
 * freeing the struct
 */
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "json.h"

static void __panic(char *fmt, ...) {
    va_list va;
    char msg[1024];

    va_start(va, fmt);
    vsnprintf(msg, sizeof(msg), fmt, va);
    fprintf(stderr, "%s\n", msg);

    va_end(va);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    FILE *fp;
    struct stat st;
    char *raw_json;

    if (argc == 1) {
        __panic("Usage: %s <file>\n", argv[0]);
    }

    if (stat(argv[1], &st) == -1) {
        __panic("Failed to stat file: %s\n", strerror(errno));
    }

    if ((fp = fopen(argv[1], "r")) == NULL) {
        __panic("Failed to open file: %s\n", strerror(errno));
    }

    raw_json = malloc(sizeof(char) * st.st_size);

    if (fread(raw_json, 1, st.st_size, fp) != st.st_size) {
        __panic("Failed to read file: %s\n", strerror(errno));
    }

    raw_json[st.st_size] = '\0';

    clock_t start_parse = clock();
    json *J = jsonParse(raw_json);
    clock_t end_parse = clock();
    long double elapsed_ms = (double)(end_parse - start_parse) /
            CLOCKS_PER_SEC * 1000;

    free(raw_json);
    jsonPrint(J);

    printf("%s\n", jsonToString(J, NULL));
    clock_t start_free = clock();
    jsonRelease(J);
    clock_t end_free = clock();
    long double elapsed_free = (double)(end_free - start_free) /
            CLOCKS_PER_SEC * 1000;

    printf("parsed in: %0.10Lfms\n", elapsed_ms);
    printf("freed in:  %0.10Lfms\n", elapsed_free);
}
