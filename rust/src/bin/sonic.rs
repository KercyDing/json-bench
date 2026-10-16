#[path = "../common.rs"]
mod common;

use sonic_rs::{JsonContainerTrait, JsonValueTrait, Value};

/// Visits every value in the tree once, folding a bit pattern into the sum.
fn scan(value: &Value) -> u64 {
    let mut acc: u64 = 0;
    walk(value, &mut acc);
    acc
}

fn walk(value: &Value, acc: &mut u64) {
    if let Some(object) = value.as_object() {
        for (_key, member) in object.iter() {
            walk(member, acc);
        }
    } else if let Some(array) = value.as_array() {
        for element in array.iter() {
            walk(element, acc);
        }
    } else if let Some(number) = value.as_f64() {
        *acc = acc.wrapping_add(number.to_bits());
    } else if let Some(text) = value.as_str() {
        *acc = acc.wrapping_add(text.len() as u64);
    } else {
        *acc = acc.wrapping_add(1);
    }
}

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
        let elapsed = common::bench_scan(&value, repeats, scan);
        common::print_result("sonic-rs", "scan", input.len(), repeats, elapsed);
    }
}
