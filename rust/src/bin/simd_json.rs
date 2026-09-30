#[path = "../common.rs"]
mod common;

fn main() {
    println!("simd-json benchmark");
    println!("data: data/json, input read and cleanup excluded");

    for name in common::DATASETS {
        let input = common::read_file(name);
        let repeats = common::repeat_count(input.len());
        common::print_dataset(name, input.len(), repeats);

        // simd-json parses in place, so every sample owns a mutable copy of the input.
        let elapsed = common::bench(&input, || {
            let mut buffer = input.clone();
            simd_json::to_owned_value(&mut buffer).expect("valid JSON")
        });
        common::print_result("simd-json", "arbitrary-decode", input.len(), repeats, elapsed);

        let elapsed = common::bench(&input, || {
            let mut buffer = input.clone();
            let value = simd_json::to_owned_value(&mut buffer).expect("valid JSON");
            simd_json::to_string(&value).expect("serializable")
        });
        common::print_result("simd-json", "transform", input.len(), repeats, elapsed);

        let mut buffer = input.clone();
        let value = simd_json::to_owned_value(&mut buffer).expect("valid JSON");
        let elapsed = common::bench_get(&value, common::path_for(name), repeats);
        common::print_result("simd-json", "get", input.len(), repeats * common::GET_BATCH, elapsed);
    }
}
