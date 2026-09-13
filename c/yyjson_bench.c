#include "yyjson.h"

#include "bench_paths.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef BENCH_DATA_DIR
#define BENCH_DATA_DIR "data/json"
#endif

static const char *const datasets[] = {
    "canada.json", "citm_catalog.json", "fgo.json", "github_events.json",
    "gsoc-2018.json", "lottie.json", "otfcc.json", "poet.json",
    "twitter.json", "twitterescaped.json",
};

static uint64_t now_ns(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000ULL + (uint64_t)time.tv_nsec;
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

static const bench_path *find_path(const char *name) {
    for (size_t i = 0; i < sizeof(bench_paths) / sizeof(bench_paths[0]); i++) {
        if (strcmp(bench_paths[i].dataset, name) == 0) return &bench_paths[i];
    }
    return NULL;
}

/// Walks the precompiled path with yyjson's own accessors.
static yyjson_val *get_target(yyjson_val *root, const bench_path *path) {
    yyjson_val *current = root;
    for (size_t i = 0; i < path->count; i++) {
        const bench_step *step = &path->steps[i];
        current = step->is_index ? yyjson_arr_get(current, step->index) : yyjson_obj_get(current, step->field);
        if (current == NULL) return NULL;
    }
    return current;
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

#define GET_BATCH 1024

static uint64_t bench_get(yyjson_val *root, const bench_path *path, size_t repeats) {
    black_box(get_target(root, path));

    uint64_t elapsed = 0;
    for (size_t i = 0; i < repeats; i++) {
        const uint64_t start = now_ns();
        for (size_t b = 0; b < GET_BATCH; b++) black_box(get_target(root, path));
        const uint64_t end = now_ns();
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

        const bench_path *path = find_path(name);
        yyjson_doc *document = yyjson_read(input, size, 0);
        if (document == NULL || path == NULL) {
            fprintf(stderr, "failed to load %s for get\n", name);
            free(input);
            return 1;
        }
        print_result("get", size, repeats * GET_BATCH, bench_get(yyjson_doc_get_root(document), path, repeats));
        yyjson_doc_free(document);

        free(input);
    }
    return 0;
}
