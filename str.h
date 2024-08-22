
#pragma once

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct str {
    size_t len;
    size_t cap;
    char* data;
};

static struct str str_slice(struct str str, size_t begin, size_t end)
{
    return (struct str) {
        .len = end - begin,
        .data = str.data + begin
    };
}

static void str_free(struct str* str)
{
    free(str->data);
    str->len = 0;
    str->cap = 0;
}

static int str_right_pad(struct str* str, char ch, size_t n)
{
    struct str restore = *str;
    int retval = 0;

    size_t new_len = str->len + n;
    if (new_len > str->cap) {
        str->cap *= 2;
        void* tmp = realloc(str, str->cap);
        if (!tmp) {
            retval = -errno;
            goto fail;
        }
        str->data = tmp;
    }

    for (size_t i = str->len; i < new_len; i++) {
        str->data[i] = ch;
    }
    str->len = new_len;
    return retval;

fail:
    *str = restore;
    return retval;
}

static void str_println(struct str str, FILE* f)
{
    for (size_t i = 0; i < str.len; i++) {
        fputc(str.data[i], f);
    }
    fputc('\n', f);
}

static int str_append(struct str* str, int ch)
{
    struct str restore = *str;
    if (str->len + 1 > str->cap) {
        str->cap *= 2;
        void* tmp = realloc(str->data, str->cap);
        if (!tmp) {
            *str = restore;
            return -1;
        }
        str->data = tmp;
    }
    str->data[str->len++] = ch;
    return 0;
}

/* read contents from file `f` and return as str */
static struct str read_all(FILE* f)
{
    struct str str = {
        .cap = 4096,
        .len = 0,
        .data = NULL,
    };
    str.data = malloc(str.cap);
    if (str.data == NULL) {
        perror("malloc");
        return (struct str) { 0 };
    }

    while (1) {
        str.len += fread(&str.data[str.len], 1, str.cap - str.len, f);
        if (str.len != str.cap) {
            if (feof(f)) {
                break;
            } else if (ferror(f)) {
                perror("fread");
                goto fail;
            } else {
                /* programming error */
                fprintf(stderr, "whoopsie!\n");
                abort();
            }
        }
        str.cap *= 2;
        void* tmp = realloc(str.data, str.cap);
        if (!tmp) {
            perror("realloc");
            goto fail;
        }
        str.data = tmp;
    }

    str.len -= 1;

    /* shrink to what's needed */
    void* tmp = realloc(str.data, str.len);
    if (tmp) {
        /* it's ok if shrinking fails */
        str.data = tmp;
        str.cap = str.len;
    }

    return str;

fail:
    free(str.data);
    return (struct str) { 0 };
}

/* read contents from file `f`, but first apply `map` to characters and then
 * filter them with `filter` */
static struct str read_all_filter(FILE* f, int (*filter)(int ch), int (*map)(int ch))
{
    struct str str = {
        .data = NULL,
        .cap = 4096,
        .len = 0,
    };
    str.data = malloc(str.cap);
    if (str.data == NULL) {
        perror("malloc");
        goto malloc_failed;
    }

    int c;
    while ((c = fgetc(f)) != EOF) {
        c = map(c);

        if (!filter(c)) {
            continue;
        }

        int ok = str_append(&str, c);
        if (ok == -1) {
            goto fail;
        }
    }

    return str;

fail:
    free(str.data);
malloc_failed:
    return (struct str) { 0 };
}
