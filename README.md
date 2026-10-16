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
| `jsonz` | Zig | [KercyDing/jsonz](https://github.com/KercyDing/jsonz) 0.10.0 | all |
| `std.json` | Zig | Zig standard library | all |
| `serde` | Zig | [OrlovEvgeny/serde.zig](https://github.com/OrlovEvgeny/serde.zig) | encode, decode |
| `json.zig` | Zig | [lalinsky/json.zig](https://github.com/lalinsky/json.zig), pinned commit `0813fe1` | encode, decode |
| `yyjson` | C | [ibireme/yyjson](https://github.com/ibireme/yyjson) | load, transform, scan |
| `simdjson` | C++ | [simdjson/simdjson](https://github.com/simdjson/simdjson) | load, transform, scan |
| `glaze` | C++ | [stephenberry/glaze](https://github.com/stephenberry/glaze) | load, transform, scan |
| `sonic-rs` | Rust | [bytedance/sonic-rs](https://github.com/bytedance/sonic-rs) | load, transform, scan |
| `simd-json` | Rust | [simd-lite/simd-json](https://github.com/simd-lite/simd-json) | load, transform, scan |

`serde.zig` and `json.zig` support typed tasks only. `json.zig` is pinned for
Zig 0.16/0.17 compatibility. `simdjson` uses `minify()` for transform.

## Tasks

| Task | Token | Timed operation |
| --- | --- | --- |
| Encode known data | `known-encode` | Typed Zig value → JSON. |
| Decode known data | `known-decode` | JSON → typed Zig value, skipping unknown fields. |
| Load arbitrary data | `arbitrary-decode` | JSON → DOM. |
| Transform data | `transform` | JSON → DOM → JSON, without edits. |
| Scan document | `scan` | Traverse the DOM and accumulate scalar values into a checksum. |

Typed tasks use shared Zig schemas. Scan visits every value through the
library's own iteration API; object keys are excluded.

## Methodology

- **Timing boundaries.** File I/O and result cleanup are excluded. Encode
  prepares its typed value once; scan parses once. Other tasks parse anew on
  each iteration.
- **Metrics.** `ns/op` and `ops/s` refer to complete operations. `GB/s` uses
  output bytes for encode and input bytes otherwise; for scan, one operation
  traverses one document.
- **Input ownership is not normalized.** For load, `jsonz` parses a reusable
  mutable, padded buffer restored before timing; for transform, it copies the
  input inside the timed region. `simdjson` builds a fresh `padded_string` and
  `simd-json` clones the buffer inside timing. Each adapter uses its library's
  allocation strategy rather than a shared allocator.
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
