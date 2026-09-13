#pragma once

#include <stdbool.h>
#include <stddef.h>

// Precompiled access steps for the `get` task, so libraries without an RFC 6901
// pointer API can walk to the target with their own object/array accessors.
typedef struct {
    bool is_index;
    const char *field;
    size_t index;
} bench_step;

typedef struct {
    const char *dataset;
    const bench_step *steps;
    size_t count;
} bench_path;

#define BENCH_COUNT(steps) (sizeof(steps) / sizeof((steps)[0]))

static const bench_step bench_canada_steps[] = {
    {false, "features", 0}, {true, NULL, 0},    {false, "geometry", 0},
    {false, "coordinates", 0}, {true, NULL, 0}, {true, NULL, 0},
};
static const bench_step bench_citm_steps[] = {
    {false, "areaNames", 0}, {false, "205705993", 0},
};
static const bench_step bench_fgo_steps[] = {
    {false, "mstSvt", 0}, {true, NULL, 0}, {false, "relateQuestIds", 0}, {true, NULL, 0},
};
static const bench_step bench_github_steps[] = {
    {true, NULL, 0}, {false, "actor", 0}, {false, "login", 0},
};
static const bench_step bench_gsoc_steps[] = {
    {false, "0", 0}, {false, "name", 0},
};
static const bench_step bench_lottie_steps[] = {
    {false, "assets", 0}, {true, NULL, 0}, {false, "layers", 0}, {true, NULL, 0}, {false, "nm", 0},
};
static const bench_step bench_otfcc_steps[] = {
    {false, "head", 0}, {false, "version", 0},
};
static const bench_step bench_poet_steps[] = {
    {true, NULL, 0}, {false, "name", 0},
};
static const bench_step bench_twitter_steps[] = {
    {false, "statuses", 0}, {true, NULL, 0}, {false, "user", 0}, {false, "id", 0},
};

static const bench_path bench_paths[] = {
    {"canada.json", bench_canada_steps, BENCH_COUNT(bench_canada_steps)},
    {"citm_catalog.json", bench_citm_steps, BENCH_COUNT(bench_citm_steps)},
    {"fgo.json", bench_fgo_steps, BENCH_COUNT(bench_fgo_steps)},
    {"github_events.json", bench_github_steps, BENCH_COUNT(bench_github_steps)},
    {"gsoc-2018.json", bench_gsoc_steps, BENCH_COUNT(bench_gsoc_steps)},
    {"lottie.json", bench_lottie_steps, BENCH_COUNT(bench_lottie_steps)},
    {"otfcc.json", bench_otfcc_steps, BENCH_COUNT(bench_otfcc_steps)},
    {"poet.json", bench_poet_steps, BENCH_COUNT(bench_poet_steps)},
    {"twitter.json", bench_twitter_steps, BENCH_COUNT(bench_twitter_steps)},
    {"twitterescaped.json", bench_twitter_steps, BENCH_COUNT(bench_twitter_steps)},
};
