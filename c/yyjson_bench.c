#include "yyjson.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#ifndef BENCH_DATA_DIR
#define BENCH_DATA_DIR "data/json"
#endif

static const char *const datasets[] = {
    "canada.json", "citm_catalog.json", "fgo.json", "github_events.json",
    "gsoc-2018.json", "lottie.json", "otfcc.json", "poet.json",
    "twitter.json", "twitterescaped.json",
};

/// Monotonic nanoseconds. Windows uses the same counter libc++'s
/// `steady_clock` uses, so the C and C++ rows measure with one clock; mingw's
/// `clock_gettime` shim wants `clock_gettime64`, which arm64 lacks.
static uint64_t now_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    const uint64_t seconds = (uint64_t)(counter.QuadPart / frequency.QuadPart);
    const uint64_t fraction = (uint64_t)(counter.QuadPart % frequency.QuadPart);
    return seconds * 1000000000ULL + fraction * 1000000000ULL / (uint64_t)frequency.QuadPart;
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000ULL + (uint64_t)time.tv_nsec;
#endif
}

static size_t repeat_count(size_t size) {
    if (size <= 256) return 100000;
    const size_t target = 64 * 1024 * 1024;
    if (size == 0 || size >= target) return 1;
    return (target + size - 1) / size;
}

static char *read_file(const char *name, size_t *size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", BENCH_DATA_DIR, name);

    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    fseek(file, 0, SEEK_END);
    *size = (size_t)ftell(file);
    rewind(file);

    char *data = malloc(*size);
    if (data == NULL || fread(data, 1, *size, file) != *size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return data;
}

static void black_box(const void *value) {
    __asm__ __volatile__("" : : "g"(value) : "memory");
}

/// Visits every value in the tree once, folding a bit pattern into `acc`.
static uint64_t scan_value(yyjson_val *value) {
    uint64_t acc = 0;
    if (yyjson_is_obj(value)) {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(value, &iter);
        yyjson_val *key;
        while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
            acc += scan_value(yyjson_obj_iter_get_val(key));
        }
    } else if (yyjson_is_arr(value)) {
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(value, &iter);
        yyjson_val *item;
        while ((item = yyjson_arr_iter_next(&iter)) != NULL) {
            acc += scan_value(item);
        }
    } else if (yyjson_is_num(value)) {
        const double number = yyjson_get_num(value);
        uint64_t bits;
        memcpy(&bits, &number, sizeof(bits));
        acc += bits;
    } else if (yyjson_is_str(value)) {
        acc += (uint64_t)yyjson_get_len(value);
    } else {
        acc += 1;
    }
    return acc;
}

static uint64_t bench_decode(const char *input, size_t size, size_t repeats) {
    yyjson_doc *warmup = yyjson_read(input, size, 0);
    if (warmup == NULL) return 0;
    yyjson_doc_free(warmup);

    uint64_t elapsed = 0;
    for (size_t i = 0; i < repeats; i++) {
        const uint64_t start = now_ns();
        yyjson_doc *document = yyjson_read(input, size, 0);
        const uint64_t end = now_ns();
        if (document == NULL) return 0;
        black_box(yyjson_doc_get_root(document));
        yyjson_doc_free(document);
        elapsed += end > start ? end - start : 1;
    }
    return elapsed;
}

static uint64_t bench_transform(const char *input, size_t size, size_t repeats) {
    yyjson_doc *warmup = yyjson_read(input, size, 0);
    if (warmup == NULL) return 0;
    yyjson_doc_free(warmup);

    uint64_t elapsed = 0;
    for (size_t i = 0; i < repeats; i++) {
        const uint64_t start = now_ns();
        yyjson_doc *document = yyjson_read(input, size, 0);
        if (document == NULL) return 0;
        size_t output_size = 0;
        char *output = yyjson_write(document, 0, &output_size);
        const uint64_t end = now_ns();
        if (output == NULL) {
            yyjson_doc_free(document);
            return 0;
        }
        black_box(output);
        free(output);
        yyjson_doc_free(document);
        elapsed += end > start ? end - start : 1;
    }
    return elapsed;
}

static uint64_t bench_scan(yyjson_val *root, size_t repeats) {
    uint64_t elapsed = 0;
    for (size_t i = 0; i < repeats; i++) {
        const uint64_t start = now_ns();
        const uint64_t acc = scan_value(root);
        const uint64_t end = now_ns();
        black_box(&acc);
        elapsed += end > start ? end - start : 1;
    }
    return elapsed;
}

static void print_result(const char *task, size_t size, size_t repeats, uint64_t elapsed) {
    const double milliseconds = (double)elapsed / (double)repeats / 1000000.0;
    const double mib_per_second =
        (double)size * 1000000000.0 / (double)elapsed * (double)repeats / (1024.0 * 1024.0);
    printf("  yyjson %s: %.6f ms/op, %.2f MiB/s\n", task, milliseconds, mib_per_second);
}

int main(void) {
    printf("yyjson benchmark\n");
    printf("data: data/json, input read and cleanup excluded\n");

    for (size_t i = 0; i < sizeof(datasets) / sizeof(datasets[0]); i++) {
        const char *name = datasets[i];
        size_t size = 0;
        char *input = read_file(name, &size);
        if (input == NULL) {
            fprintf(stderr, "failed to read %s\n", name);
            return 1;
        }

        const size_t repeats = repeat_count(size);
        printf("\n%s (%zu bytes, %zu repeats)\n", name, size, repeats);

        const uint64_t decode = bench_decode(input, size, repeats);
        if (decode == 0) {
            fprintf(stderr, "failed to parse %s\n", name);
            free(input);
            return 1;
        }
        print_result("arbitrary-decode", size, repeats, decode);

        const uint64_t transform = bench_transform(input, size, repeats);
        if (transform == 0) {
            fprintf(stderr, "failed to transform %s\n", name);
            free(input);
            return 1;
        }
        print_result("transform", size, repeats, transform);

        yyjson_doc *document = yyjson_read(input, size, 0);
        if (document == NULL) {
            fprintf(stderr, "failed to load %s for scan\n", name);
            free(input);
            return 1;
        }
        print_result("scan", size, repeats, bench_scan(yyjson_doc_get_root(document), repeats));
        yyjson_doc_free(document);

        free(input);
    }
    return 0;
}
