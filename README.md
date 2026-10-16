# json-bench

<p>
  <img src="https://github.com/KercyDing/json-bench/releases/download/20261004-52c64c6/ranking.png" />
</p>

Benchmarks for JSON libraries across Zig, C, C++, and Rust, on real-world data.

Each implementation is compared per task, and each chart only includes the
implementations that support that task. Typed tasks compare the Zig libraries;
DOM tasks also bring in native C, C++, and Rust parsers.

## Implementations

| Name | Language | Project | Tasks |
| --- | --- | --- | --- |
| `jsonz` | Zig | [KercyDing/jsonz](https://github.com/KercyDing/jsonz) 0.9.2 | all |
| `std.json` | Zig | Zig standard library | all |
| `serde` | Zig | [OrlovEvgeny/serde.zig](https://github.com/OrlovEvgeny/serde.zig) | encode, decode |
| `json.zig` | Zig | [lalinsky/json.zig](https://github.com/lalinsky/json.zig), pinned commit `0813fe1` | encode, decode |
| `yyjson` | C | [ibireme/yyjson](https://github.com/ibireme/yyjson) | load, transform, scan |
| `simdjson` | C++ | [simdjson/simdjson](https://github.com/simdjson/simdjson) | load, transform, scan |
| `glaze` | C++ | [stephenberry/glaze](https://github.com/stephenberry/glaze) | load, transform, scan |
| `sonic-rs` | Rust | [bytedance/sonic-rs](https://github.com/bytedance/sonic-rs) | load, transform, scan |
| `simd-json` | Rust | [simd-lite/simd-json](https://github.com/simd-lite/simd-json) | load, transform, scan |

`serde.zig` and `json.zig` have no DOM, so they are left out of the DOM tasks
instead of being compared on an API they do not have. `json.zig` uses
`decodeFromSliceLeaky` with the iteration arena (strings are copied) and
`encode` into an allocating writer. Unknown fields are skipped to match the
shared partial schemas. `simdjson`'s DOM is immutable — there is no
way to edit a parsed tree — so its `transform` serializes what it parsed with
`minify()`.

json.zig is pinned to a post-0.1.0 commit for Zig 0.16/0.17 build-script
compatibility; the 0.1.0 release build script does not support Zig 0.17.

## Tasks

| Task | Token | What it measures |
| --- | --- | --- |
| Encode known data | `known-encode` | Serialize a typed Zig value. |
| Decode known data | `known-decode` | Parse into a typed Zig value. |
| Load arbitrary data | `arbitrary-decode` | Parse into a DOM. |
| Transform data | `transform` | Parse into a DOM and serialize it back. |
| Scan document | `scan` | Walk every value of the parsed DOM through the library's own iteration API. |

The first two need a schema, so they only run for the Zig libraries.

## Methodology

- **One-shot owning parse.** Each timed iteration creates the storage that owns
  the parsed result. Reading the input and freeing the result are excluded.
- **Input ownership is not normalized.** `yyjson_read` lets the DOM's strings
  point into the caller's input, so it never copies the input; `jsonz` copies it
  (in-place escape decoding needs a mutable, padded copy), `simdjson` builds a
  fresh `padded_string`, and `simd-json` clones the buffer. The timed region
  includes whatever the implementation itself does, copy included — this is a
  real design difference, not a shared cost. Every implementation also runs on
  the platform's default allocator, though `simd-json` asks for
  `mimalloc`/`jemalloc` and `snmalloc`.
- **`scan` times traversal, not parsing.** It parses each dataset once, keeps
  the document alive, and times whole traversals of it; the other tasks time the
  parse, the serialize, or both, per sample.
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
by median, writes `results/{summary.csv,summary.md,index.html}`, and opens
the report.

On Windows the C/C++ build uses `release-windows`, the same preset with the
`Ninja` generator, since `make` is not available there.

## Options

| Option | Description |
| --- | --- |
| `--runs N` | Independent process runs per implementation (default: 3). |
| `--parallel [THREADS]` | Also measure 1, 2, 4, ... processes at once, up to `THREADS`, and chart the scaling. |
| `--no-build` | Regenerate the reports from `results/measurements.json`. |

## Layout

| Path | Contents |
| --- | --- |
| `bench.py` | Build, run, and report driver. Records the CPU and tool versions of the machine in `measurements.json`. |
| `rank.py` | Ranks published results per platform and merged over the DOM tasks (`--all-tasks` adds the typed Zig-only ones), and draws the ranking with `--png` (`python rank.py ~/Downloads --png ranking.png`). |
| `CMakePresets.json` | The `release` preset (`Unix Makefiles`, `Release`, `build/`) shared by `bench.py` and editors; keeps cached generator/build type from drifting. `release-windows` is the same preset with `Ninja`. |
| `zig/` | jsonz, std.json, serde.zig and json.zig adapters, sharing the `bench.zig` harness. |
| `c/` | yyjson benchmark. |
| `cpp/` | simdjson and Glaze benchmarks. |
| `rust/` | sonic-rs and simd-json benchmarks. |
| `data/json/` | Corpus. |
| `results/` | Generated reports (gitignored). |

## Scan

The `scan` task parses each dataset once, then times full traversals of the
parsed document. Every implementation visits each value once: objects and
arrays are walked, numbers fold the bit pattern of their `f64` value into the
checksum, strings fold their byte length, and `true`, `false` and `null` fold
`1`. The traversal goes through each library's own iteration API, so the number
reflects the DOM layout as much as the accessors, and the reported `GB/s` is
the document's bytes per second of traversal.

