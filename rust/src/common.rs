use std::fs;
use std::hint::black_box;
use std::path::PathBuf;
use std::time::Instant;

pub const DATASETS: &[&str] = &[
    "canada.json",
    "citm_catalog.json",
    "fgo.json",
    "github_events.json",
    "gsoc-2018.json",
    "lottie.json",
    "otfcc.json",
    "poet.json",
    "twitter.json",
    "twitterescaped.json",
];

pub fn read_file(name: &str) -> Vec<u8> {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../data/json")
        .join(name);
    fs::read(path).expect("benchmark dataset must be readable")
}

pub fn repeat_count(size: usize) -> usize {
    if size <= 256 {
        100000
    } else {
        const TARGET: usize = 64 * 1024 * 1024;
        if size >= TARGET {
            1
        } else {
            (TARGET + size - 1) / size
        }
    }
}

pub fn bench<T>(input: &[u8], mut parse: impl FnMut() -> T) -> u128 {
    black_box(parse());

    let repeats = repeat_count(input.len());
    let mut elapsed = 0;
    for _ in 0..repeats {
        let start = Instant::now();
        let value = parse();
        black_box(&value);
        elapsed += start.elapsed().as_nanos().max(1);
        drop(value);
    }
    elapsed
}

/// Times `repeats` full-tree scans of an already parsed document.
pub fn bench_scan<V>(root: &V, repeats: usize, mut scan: impl FnMut(&V) -> u64) -> u128 {
    black_box(scan(root));

    let mut elapsed = 0;
    for _ in 0..repeats {
        let start = Instant::now();
        let acc = scan(root);
        elapsed += start.elapsed().as_nanos().max(1);
        black_box(acc);
    }
    elapsed
}

pub fn print_dataset(name: &str, size: usize, repeats: usize) {
    println!("\n{name} ({size} bytes, {repeats} repeats)");
}

pub fn print_result(parser: &str, task: &str, size: usize, repeats: usize, elapsed: u128) {
    let milliseconds = elapsed as f64 / repeats as f64 / 1_000_000.0;
    let mib_per_second =
        size as f64 * 1_000_000_000.0 / elapsed as f64 * repeats as f64 / 1024.0 / 1024.0;
    println!("  {parser} {task}: {milliseconds:.6} ms/op, {mib_per_second:.2} MiB/s");
}
