# zig-serde-bench

Benchmarks for JSON libraries using real-world data. Typed work is compared
between the Zig libraries; DOM work also brings in native C, C++, and Rust
parsers.

## Implementations

| Name | Language | Project | Tasks |
| --- | --- | --- | --- |
| `serde` | Zig | [serde.zig](https://github.com/OrlovEvgeny/serde.zig) | encode, decode |
| `jsonz` | Zig | [KercyDing/jsonz](https://github.com/KercyDing/jsonz) | all |
| `std.json` | Zig | Zig standard library | all |
| `yyjson` | C | [ibireme/yyjson](https://github.com/ibireme/yyjson) | load, transform, get |
| `simdjson` | C++ | [simdjson/simdjson](https://github.com/simdjson/simdjson) | load, get |
| `glaze` | C++ | [stephenberry/glaze](https://github.com/stephenberry/glaze) | load, transform, get |
| `sonic-rs` | Rust | [bytedance/sonic-rs](https://github.com/bytedance/sonic-rs) | load, transform, get |

Each chart only includes the implementations that support the task.

## Tasks

| Task | Token | What it measures |
| --- | --- | --- |
| Encode known data | `known-encode` | Serialize a typed Zig value. |
| Decode known data | `known-decode` | Parse into a typed Zig value. |
| Load arbitrary data | `arbitrary-decode` | Parse into a DOM. |
| Transform data | `transform` | Parse into a DOM and serialize it back. |
| Get element | `get` | Read one nested element through the library's own access API. |

The first two need a schema, so they only compare the Zig libraries. `serde.zig`
has no DOM, so it is left out of the DOM tasks instead of being compared on an
API it does not have. `simdjson` is read-only, so it has no `transform`.

Timings exclude reading the input and freeing the parsed document. Each
iteration is a one-shot owning parse: implementations create the storage that
owns the result inside the timed region. `simd-json` takes a mutable buffer, so
its benchmark copies the input inside each measured iteration.

## Quick start

The Zig, Rust, and Python versions are pinned in `mise.toml`; CMake downloads
simdjson and Glaze, and Cargo downloads the Rust crates.

```sh
python3 bench.py
```

`bench.py` builds every language, runs each implementation once per run in
separate processes, aggregates runs by median, writes
`results/json/{summary.csv,summary.md,index.html}`, and opens the report.

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
| `zig/` | jsonz, serde.zig and std.json adapters, sharing the `bench.zig` harness. |
| `c/` | yyjson benchmark and the vendored yyjson sources. |
| `cpp/` | simdjson and Glaze benchmarks. |
| `rust/` | sonic-rs benchmark. |
| `data/json/` | Corpus. |
| `results/` | Generated reports (gitignored). |

The `get` task reads one nested element per dataset with each library's own
access API: `ptrGet` (RFC 6901) for jsonz, `at_pointer` for simdjson, and the
native object/array accessors for the rest.

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

## Development

```sh
mise run fmt      # zig fmt
mise run build    # zig build
mise run ci       # fmt + build
```
