# json-bench

Benchmarks for JSON libraries across Zig, C, C++, and Rust, on real-world data.

Each implementation is compared per task, and each chart only includes the
implementations that support that task. Typed tasks compare the Zig libraries;
DOM tasks also bring in native C, C++, and Rust parsers.

## Implementations

| Name | Language | Project | Tasks |
| --- | --- | --- | --- |
| `jsonz` | Zig | [KercyDing/jsonz](https://github.com/KercyDing/jsonz) | all |
| `std.json` | Zig | Zig standard library | all |
| `serde` | Zig | [OrlovEvgeny/serde.zig](https://github.com/OrlovEvgeny/serde.zig) | encode, decode |
| `yyjson` | C | [ibireme/yyjson](https://github.com/ibireme/yyjson) | load, transform, get |
| `simdjson` | C++ | [simdjson/simdjson](https://github.com/simdjson/simdjson) | load, get |
| `glaze` | C++ | [stephenberry/glaze](https://github.com/stephenberry/glaze) | load, transform, get |
| `sonic-rs` | Rust | [bytedance/sonic-rs](https://github.com/bytedance/sonic-rs) | load, transform, get |

`serde.zig` has no DOM, so it is left out of the DOM tasks instead of being
compared on an API it does not have. `simdjson` is read-only, so it has no
`transform`.

## Tasks

| Task | Token | What it measures |
| --- | --- | --- |
| Encode known data | `known-encode` | Serialize a typed Zig value. |
| Decode known data | `known-decode` | Parse into a typed Zig value. |
| Load arbitrary data | `arbitrary-decode` | Parse into a DOM. |
| Transform data | `transform` | Parse into a DOM and serialize it back. |
| Get element | `get` | Read one nested element through the library's own access API. |

The first two need a schema, so they only run for the Zig libraries.

## Methodology

- **One-shot owning parse.** Each timed iteration creates the storage that owns
  the parsed result. Reading the input and freeing the result are excluded.
- **Input ownership is not normalized.** `yyjson_read` lets the DOM's strings
  point into the caller's input, so it never copies the input; `jsonz` copies it
  (in-place escape decoding needs a mutable, padded copy), `simdjson` builds a
  fresh `padded_string`, and `simd-json` clones the buffer. The timed region
  includes whatever the implementation itself does, copy included — this is a
  real design difference, not a shared cost.
- **`get` amortizes the clock.** A single access is only tens of nanoseconds, so
  every sample resolves the element 1024 times between the two clock reads. The
  clock overhead is then under 0.1% of the reported `ns/op`. The other tasks are
  millisecond-scale and time one operation per sample.
- **Processes and medians.** Each implementation runs in its own process,
  `--runs` times, with the order rotated between runs and a warmup before each
  thread count; results are medians across processes.

## Quick start

`mise` pins the Zig version; CMake fetches simdjson and Glaze, and Cargo
fetches the Rust crates, so the first run needs network access.

```sh
python3 bench.py
```

`bench.py` builds every language, runs each implementation, aggregates the runs
by median, writes `results/json/{summary.csv,summary.md,index.html}`, and opens
the report.

## Options

| Option | Description |
| --- | --- |
| `--runs N` | Independent process runs per implementation (default: 3). |
| `--parallel [THREADS]` | Also measure 1, 2, 4, ... processes at once, up to `THREADS`, and chart the scaling. |
| `--no-build` | Regenerate the reports from `results/json/measurements.json`. |

## Layout

| Path | Contents |
| --- | --- |
| `bench.py` | Build, run, and report driver. |
| `zig/` | jsonz, std.json and serde.zig adapters, sharing the `bench.zig` harness. |
| `c/` | yyjson benchmark and the vendored yyjson sources. |
| `cpp/` | simdjson and Glaze benchmarks. |
| `rust/` | sonic-rs benchmark. |
| `data/json/` | Corpus. |
| `results/` | Generated reports (gitignored). |

## Get targets

The `get` task reads one nested element per dataset with each library's own
access API: `ptrGet` (RFC 6901) for jsonz, `at_pointer` for simdjson, and native
object/array accessors for the rest.

| Dataset | Pointer |
| --- | --- |
| `canada.json` | `/features/0/geometry/coordinates/0/0` |
| `citm_catalog.json` | `/areaNames/205705993` |
| `fgo.json` | `/mstSvt/0/relateQuestIds/0` |
| `github_events.json` | `/0/actor/login` |
| `gsoc-2018.json` | `/0/name` |
| `lottie.json` | `/assets/0/layers/0/nm` |
| `otfcc.json` | `/head/version` |
| `poet.json` | `/0/name` |
| `twitter.json` | `/statuses/0/user/id` |
| `twitterescaped.json` | `/statuses/0/user/id` |
