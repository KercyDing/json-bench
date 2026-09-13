#[path = "../common.rs"]
mod common;

fn main() {
    println!("sonic-rs benchmark");
    println!("data: data/json, input read and cleanup excluded");

    for name in common::DATASETS {
        let input = common::read_file(name);
        let repeats = common::repeat_count(input.len());
        common::print_dataset(name, input.len(), repeats);

        let elapsed = common::bench(&input, || {
            sonic_rs::from_slice::<sonic_rs::Value>(&input).expect("valid JSON")
        });
        common::print_result("sonic-rs", "arbitrary-decode", input.len(), repeats, elapsed);

        let elapsed = common::bench(&input, || {
            let value = sonic_rs::from_slice::<sonic_rs::Value>(&input).expect("valid JSON");
            sonic_rs::to_string(&value).expect("serializable")
        });
        common::print_result("sonic-rs", "transform", input.len(), repeats, elapsed);

        let value = sonic_rs::from_slice::<sonic_rs::Value>(&input).expect("valid JSON");
        let elapsed = common::bench_get(&value, common::path_for(name), repeats);
        common::print_result("sonic-rs", "get", input.len(), repeats * common::GET_BATCH, elapsed);
    }
}
