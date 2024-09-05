
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "str.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const char charset[26] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static double charfreq_english[sizeof charset] = {
    ['A' - 'A'] = 0.082,
    ['B' - 'A'] = 0.015,
    ['C' - 'A'] = 0.028,
    ['D' - 'A'] = 0.043,
    ['E' - 'A'] = 0.127,
    ['F' - 'A'] = 0.022,
    ['G' - 'A'] = 0.020,
    ['H' - 'A'] = 0.061,
    ['I' - 'A'] = 0.070,
    ['J' - 'A'] = 0.0015,
    ['K' - 'A'] = 0.0077,
    ['L' - 'A'] = 0.040,
    ['M' - 'A'] = 0.024,
    ['N' - 'A'] = 0.067,
    ['O' - 'A'] = 0.075,
    ['P' - 'A'] = 0.019,
    ['Q' - 'A'] = 0.0095,
    ['R' - 'A'] = 0.060,
    ['S' - 'A'] = 0.063,
    ['T' - 'A'] = 0.091,
    ['U' - 'A'] = 0.028,
    ['V' - 'A'] = 0.0098,
    ['W' - 'A'] = 0.024,
    ['X' - 'A'] = 0.0015,
    ['Y' - 'A'] = 0.020,
    ['Z' - 'A'] = 0.00074,
};

static int do_nothing(int ch)
{
    return ch;
}

static int charset_contains(int ch)
{
    return ch >= 'A' && ch <= 'Z';
}

static size_t charset_index(char ch)
{
    if (isalpha(ch)) {
        return toupper(ch) - 'A';
    }
    fprintf(stderr, "%s: invalid char %d\n", __func__, ch);
    abort();
}

/* calculate index of coincidence of `text`
 *
 * map will transform the characters before calculating the ioc. For example,
 * ioc(data, ..., tolower) will transform samples with tolower before checking
 * if they are equal
 * */
static double ioc(struct str text, int stride, int offset, int (*map)(int))
{
    assert(offset < stride);
    if (stride > text.len) {
        return NAN;
    }
    if (text.len < 1) {
        return NAN;
    }

    int samples = 2048;
    int matches = 0;

    if (map == NULL) {
        map == do_nothing;
    }

    for (int i = 0; i < samples; i++) {
        size_t rand_a = (rand() % (text.len / stride)) * stride + offset;
        size_t rand_b;
        do {
            rand_b = (rand() % (text.len / stride)) * stride + offset;
        } while (rand_a == rand_b);

        char a = map(text.data[rand_a]);
        char b = map(text.data[rand_b]);

        if (a == b) {
            matches++;
        }
    }

    return (double)matches / (double)(samples);
}

static void frequency_count(double output[static sizeof charset], const struct str text, size_t offset, size_t stride)
{
    for (size_t i = 0; i < sizeof charset; i++) {
        output[i] = 0;
    }

    assert(offset < stride);
    for (size_t i = offset; i < text.len; i += stride) {
        if (!charset_contains(text.data[i])) {
            continue;
        }
        output[charset_index(text.data[i])] += 1.0;
    }
}

static double frequency_correlation(const double a[static sizeof charset], const double b[static sizeof charset], size_t shift)
{
    double sum = 0;
    for (size_t i = 0; i < sizeof charset; i++) {
        sum += a[i] * b[(i + shift) % sizeof charset];
    }
    return sum;
}

static void frequency_print(const double freq[static sizeof charset])
{
    for (int i = 0; i < sizeof charset; i++) {
        fprintf(stderr, "[%c] = %.0lf, ", charset[i], freq[i]);
    }
}

static void vigenere_encode(struct str text, char* output, const char* key, size_t key_len, const char* charset, size_t charset_len)
{
    for (size_t i = 0; i < text.len; i++) {
        const char ch = text.data[i];
        if (charset_contains(ch)) {
            output[i] = charset[(charset_index(ch) + key[i % key_len]) % charset_len];
        }
    }
}

static void vigenere_decode(struct str text, char* output, const char* key, size_t key_len, const char* charset, size_t charset_len)
{
    for (size_t i = 0; i < text.len; i++) {
        const char ch = text.data[i];
        if (charset_contains(ch)) {
            output[i] = charset[(charset_index(ch) - key[i % key_len] + charset_len) % charset_len];
        }
    }
}

int main(int argc, char** argv)
{
    srand(time(NULL));

    FILE* f = argc < 2
        ? stdin
        : fopen(argv[1], "r");

    if (f == NULL) {
        fprintf(stderr, "couldn't open file %s%m", argv[1]);
        exit(EXIT_FAILURE);
    }

    struct str text = read_all_filter(f, charset_contains, toupper);

    if (fclose(f) != 0) {
        perror("fclose");
        /* not fatal, continue */
    }

    if (text.data == NULL) {
        exit(EXIT_FAILURE);
    }

    /* Find key length (stride)
     * ========================*/
    int key_len = 1;
    {
        /* values better than threshold immidiately break the loop */
        constexpr double threshold = 1.6;

        double best_score = -1.0;

        for (int stride = 1; stride < text.len / 2; stride++) {
            double result = 0.0;
            for (int j = 0; j < stride; j++) {
                result += ioc(text, stride, j, toupper);
            }
            result /= stride;
            result *= 26.0; /* normalization */

            if (result > best_score) {
                best_score = result;
                key_len = stride;
                if (result > threshold) {
                    break;
                }
            }
        }
        fprintf(stderr, "best stride: %i (IOC %.2lf)\n", key_len, best_score);
    }

    /* Crack caesar ciphers column wise
     * ================================ */
    char key[key_len] = {}; /* VLAs are bad but whatever */
    {
        double frequencies[sizeof charset] = { 0 };

        for (size_t col = 0; col < key_len; col++) {
            frequency_count(frequencies, text, col, key_len);

            double best = 0;
            for (size_t i = 0; i < sizeof charset; i++) {
                double n = frequency_correlation(frequencies, charfreq_english, i);
                if (n > best) {
                    key[col] = (sizeof charset - i) % sizeof charset;
                    best = n;
                }
            }
        }
    }

    /* print key */
    printf("key: ");
    for (size_t i = 0; i < key_len; i++) {
        printf("%c", charset[key[i]]);
    }
    printf(" (");
    for (size_t i = 0; i < key_len - 1; i++) {
        printf("%d, ", key[i]);
    }
    printf("%d)", key[key_len - 1]);
    printf("\n");

    vigenere_decode(text, text.data, key, key_len, charset, sizeof charset);

    //str_println(text, stdout);

    str_free(&text);

    return EXIT_SUCCESS;
}
