use std::fs;
use std::hint::black_box;
use std::ops::Index;
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

/// One precompiled access step for the `get` task.
pub enum Step {
    Field(&'static str),
    Index(usize),
}

pub const PATHS: &[(&str, &[Step])] = &[
    (
        "canada.json",
        &[
            Step::Field("features"),
            Step::Index(0),
            Step::Field("geometry"),
            Step::Field("coordinates"),
            Step::Index(0),
            Step::Index(0),
        ],
    ),
    (
        "citm_catalog.json",
        &[Step::Field("areaNames"), Step::Field("205705993")],
    ),
    (
        "fgo.json",
        &[
            Step::Field("mstSvt"),
            Step::Index(0),
            Step::Field("relateQuestIds"),
            Step::Index(0),
        ],
    ),
    (
        "github_events.json",
        &[
            Step::Index(0),
            Step::Field("actor"),
            Step::Field("login"),
        ],
    ),
    (
        "gsoc-2018.json",
        &[Step::Field("0"), Step::Field("name")],
    ),
    (
        "lottie.json",
        &[
            Step::Field("assets"),
            Step::Index(0),
            Step::Field("layers"),
            Step::Index(0),
            Step::Field("nm"),
        ],
    ),
    (
        "otfcc.json",
        &[Step::Field("head"), Step::Field("version")],
    ),
    ("poet.json", &[Step::Index(0), Step::Field("name")]),
    (
        "twitter.json",
        &[
            Step::Field("statuses"),
            Step::Index(0),
            Step::Field("user"),
            Step::Field("id"),
        ],
    ),
    (
        "twitterescaped.json",
        &[
            Step::Field("statuses"),
            Step::Index(0),
            Step::Field("user"),
            Step::Field("id"),
        ],
    ),
];

pub fn path_for(name: &str) -> &'static [Step] {
    PATHS
        .iter()
        .find(|(dataset, _)| *dataset == name)
        .map(|(_, steps)| *steps)
        .expect("dataset must have a get path")
}

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

pub fn get<'a, V>(root: &'a V, steps: &[Step]) -> &'a V
where
    V: Index<&'static str, Output = V> + Index<usize, Output = V>,
{
    let mut current = root;
    for step in steps {
        current = match step {
            Step::Field(field) => &current[*field],
            Step::Index(index) => &current[*index],
        };
    }
    current
}

pub const GET_BATCH: usize = 1024;

pub fn bench_get<V>(root: &V, steps: &[Step], repeats: usize) -> u128
where
    V: Index<&'static str, Output = V> + Index<usize, Output = V>,
{
    black_box(get(root, steps));

    let mut elapsed = 0;
    for _ in 0..repeats {
        let start = Instant::now();
        for _ in 0..GET_BATCH {
            black_box(get(root, steps));
        }
        elapsed += start.elapsed().as_nanos().max(1);
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
