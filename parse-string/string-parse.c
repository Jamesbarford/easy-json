#include <sys/stat.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __toint(p) ((p)-48)
#define __is_white_space(x) \
    ((x) == ' ' || (x) == '\n' || (x) == '\r' || (x) == '\t')
#define __isdigit(x) ((x) >= '0' && (x) <= '9')
#define __bufput(b, i, c) ((b)[(*i)++] = (c))

#define debug(...)                                                         \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

typedef struct jsonParser {
    char *buffer;
    size_t offset;
    size_t buflen;
    int errno;
} jsonParser;

static inline char
jsonUnsafePeekAt(jsonParser *p, size_t idx)
{
    return p->buffer[idx];
}

static inline char
jsonPeek(jsonParser *p)
{
    return p->buffer[p->offset];
}

static inline int
jsonCanAdvanceBy(jsonParser *p, size_t jmp)
{
    return p->offset + jmp < p->buflen;
}

static inline void
jsonUnsafeAdvanceBy(jsonParser *p, size_t jmp)
{
    p->offset += jmp;
}

static inline void
jsonAdvance(jsonParser *p)
{
    if (jsonCanAdvanceBy(p, 1)) {
        ++p->offset;
        return;
    }
    debug("SET EOF\n");
    debug("%c%c%c%c\n", p->buffer[p->offset], p->buffer[p->offset - 1],
            p->buffer[p->offset - 2], p->buffer[p->offset - 3]);
    p->errno = -1;
}

static inline unsigned int
__parseHex4(const unsigned char *buf)
{
    unsigned int hex = 0;
    unsigned char c;

    for (int i = 0; i < 4; ++i) {
        c = buf[i];
        if (__isdigit(c)) {
            hex += __toint(c);
        } else if (c >= 'A' && c <= 'F') {
            hex += 10 + c - 'A';
        } else if (c >= 'a' && c <= 'f') {
            hex += 10 + c - 'a';
        } else {
            return INT_MAX;
        }
        if (i < 3) {
            hex = hex << 4;
        }
    }

    return hex;
}

static inline void
utf8Encode(char *buffer, unsigned int codepoint, size_t *offset)
{
    if (codepoint <= 0x7F) {
        __bufput(buffer, offset, codepoint & 0xFF);
    } else if (codepoint <= 0x7FF) {
        __bufput(buffer, offset, 0xC0 | ((codepoint >> 6) & 0xFF));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        __bufput(buffer, offset, 0xE0 | ((codepoint >> 12) & 0xFF));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 6) & 0x3F));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    } else {
        __bufput(buffer, offset, 0xE0 | ((codepoint >> 18) & 0xFF));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 12) & 0x3F));
        __bufput(buffer, offset, 0x80 | ((codepoint >> 6) & 0x3F));
        __bufput(buffer, offset, 0x80 | (codepoint & 0x3F));
    }
}

static inline unsigned int
jsonParseUTF16(jsonParser *p)
{
    if (!jsonCanAdvanceBy(p, 5)) {
        debug("Cannot advance by 5\n");
        p->errno = -1;
        return 0;
    }
    unsigned int codepoint = __parseHex4(
            (const unsigned char *)p->buffer + p->offset + 1);

    if (codepoint == INT_MAX) {
        p->errno = -1;
        debug("Failed to parse hex\n");
        return 0;
    }

    /* 5 -> 'u1111' is 5 chars */
    jsonUnsafeAdvanceBy(p, 4);

    /* UTF-16 pair */
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        if (codepoint <= 0xDBFF) {
            if (!jsonCanAdvanceBy(p, 6)) {
                debug("run out of characters \n");
                p->errno = -1;
                return 0;
            }

            jsonAdvance(p);
            if (jsonPeek(p) != '\\' &&
                    jsonUnsafePeekAt(p, p->offset + 1) != 'u') {
                debug("In valid utf-16 pair\n");
                p->errno = -1;
                return 0;
            }

            jsonUnsafeAdvanceBy(p, 2);

            unsigned int codepoint2 = __parseHex4(
                    (const unsigned char *)p->buffer + p->offset);
            if (codepoint2 == INT_MAX) {
                debug("Failed to parse hex\n");
                p->errno = -1;
                return 0;
            }

            /* Invalid surrogate */
            if (codepoint2 < 0xDC00 || codepoint2 > 0xDFFF) {
                debug("Invalid surrogate\n");
                p->errno = -1;
                return 0;
            }
            codepoint = 0x10000 +
                    (((codepoint & 0x3FF) << 10) | (codepoint2 & 0x3FF));

            /*
            codepoint = (((codepoint - 0xD800) << 10) | (codepoint2 - 0xDC00)) +
                    0x10000;
            */
            jsonCanAdvanceBy(p, 3);
        } else {
            debug("??\n");
            p->errno = -1;
            return 0;
        }
    }

    return codepoint;
}

static char *
jsonParseString(jsonParser *p)
{
    int run = 1;
    size_t start = p->offset;
    size_t end = p->offset;

    if (jsonPeek(p) == '"') {
        jsonAdvance(p);
        end++;
    }

    /* approximate string length */
    while (jsonUnsafePeekAt(p, end) != '\0') {
        char cur = jsonUnsafePeekAt(p, end);
        if (cur == '"') {
            if (jsonUnsafePeekAt(p, end - 1) != '\\') {
                break;
            }
        } else if (cur == '\\') {
            // escape sequence so add another byte
            if (jsonUnsafePeekAt(p, end + 1) == '\0') {
                break;
            }
            end++;
        }
        end++;
    }

    if (jsonUnsafePeekAt(p, end) == '\0') {
        debug("end char: '%c' at '%zu'\n", jsonUnsafePeekAt(p, end), p->offset);
        p->errno = -1;
        return NULL;
    }

    size_t len = 0;
    char *str = malloc(sizeof(char) * end - start);

    while (run && jsonPeek(p) != '\0') {
        switch (jsonPeek(p)) {
        case '\\':
            jsonAdvance(p);
            switch (jsonPeek(p)) {
            case '\\':
            case '"':
            case '/':
                __bufput(str, &len, jsonPeek(p));
                break;
            case 'b':
                __bufput(str, &len, '\b');
                break;
            case 'f':
                __bufput(str, &len, '\f');
                break;
            case 'n':
                __bufput(str, &len, '\n');
                break;
            case 'r':
                __bufput(str, &len, '\r');
                break;
            case 't':
                __bufput(str, &len, '\t');
                break;
            case 'u': {
                unsigned int codepoint = jsonParseUTF16(p);
                if (p->errno != 0) {
                    debug("failed to parse UTF16 '%c%c%c%c%c%c' %zu\n",
                            jsonUnsafePeekAt(p, p->offset - 5),
                            jsonUnsafePeekAt(p, p->offset - 4),
                            jsonUnsafePeekAt(p, p->offset - 3),
                            jsonUnsafePeekAt(p, p->offset - 2),
                            jsonUnsafePeekAt(p, p->offset - 1),
                            jsonUnsafePeekAt(p, p->offset - 0), p->offset);
                    goto err;
                }
                utf8Encode(str, codepoint, &len);
                break;
            }
            }
            break;
        case '"':
            run = 0;
            break;
        default:
            __bufput(str, &len, jsonPeek(p));
            break;
        }
        jsonAdvance(p);
    }

    if (jsonPeek(p) == '"') {
        p->errno = -1;
        debug("String not ending in '\"' -> '%c' %zu\n", jsonPeek(p),
                p->offset);
        goto err;
    }

    str[len] = '\0';
    return str;

err:
    debug("String parse error\n");
    if (str) {
        free(str);
    }
    return NULL;
}

static inline unsigned char *
escapeString(char *buf)
{
    unsigned char *outbuf = NULL;
    unsigned char *ptr = (unsigned char *)buf;
    size_t len = 0;
    size_t escape_chars = 0;
    size_t offset = 0;

    if (buf == NULL) {
        return NULL;
    }

    while (*ptr) {
        switch (*ptr) {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            escape_chars++;
            break;
        default:
            if (*ptr < 32) {
                escape_chars += 6;
            }
            break;
        }
        ++ptr;
    }

    len = (size_t)(ptr - (unsigned char *)buf) + escape_chars;
    outbuf = malloc(sizeof(char) * len);

    if (escape_chars == 0) {
        memcpy(outbuf, buf, len);
        outbuf[len] = '\0';
        return outbuf;
    }

    ptr = (unsigned char *)buf;

    while (*ptr) {
        if (*ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            __bufput(outbuf, &offset, (char)*ptr);
        } else {
            __bufput(outbuf, &offset, '\\');
            switch (*ptr) {
            case '\\':
                __bufput(outbuf, &offset, '\\');
                break;
            case '\"':
                __bufput(outbuf, &offset, '\"');
                break;
            case '\b':
                __bufput(outbuf, &offset, 'b');
                break;
            case '\n':
                __bufput(outbuf, &offset, 'n');
                break;
            case '\t':
                __bufput(outbuf, &offset, 't');
                break;
            case '\f':
                __bufput(outbuf, &offset, 'f');
                break;
            case '\r':
                __bufput(outbuf, &offset, 'r');
                break;
            default:
                sprintf((char *)outbuf + offset, "u%04x", (unsigned int)*ptr);
                offset += 4;
                break;
            }
        }
        ++ptr;
    }
    outbuf[len] = '\0';
    return outbuf;
}

static void
parserInit(jsonParser *p, char *buffer, size_t buflen)
{
    p->errno = 0;
    p->buffer = buffer;
    p->buflen = buflen;
    p->offset = 0;
}

static void
parserParse(jsonParser *p, char *buffer, size_t buflen)
{
    parserInit(p, buffer, buflen);
    while (p->offset != p->buflen) {
        char *parsed = jsonParseString(p);
        if (parsed == NULL) {
            debug("offset: %zu\n", p->offset);
            printf("NULL\n");
            break;
        }
        unsigned char *escaped = escapeString(parsed);
        printf("parsed: %s\n", parsed);
        printf("FLUSH\n");
        printf("escaped: %s\n", escaped);
        free(parsed);
        free(escaped);
        assert(jsonPeek(p) == '\n');
        jsonAdvance(p);
        assert(jsonPeek(p) == '"');
    }
}

int
main(void)
{
    char *file_name = "./text.txt";
    struct stat st;
    if (stat(file_name, &st) == -1) {
        fprintf(stderr, "Cannot open file '%s'\n", file_name);
        exit(1);
    }

    FILE *fp = fopen(file_name, "r");
    jsonParser p;
    char *rawfile = malloc(sizeof(char) * st.st_size);

    assert(fread(rawfile, 1, st.st_size, fp) == st.st_size);
    rawfile[st.st_size] = '\0';
    printf("%s\n", rawfile);

    parserParse(&p, rawfile, st.st_size);
    free(rawfile);
}
