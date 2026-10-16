#[path = "../common.rs"]
mod common;

use simd_json::prelude::*;

/// Visits every value in the tree once, folding a bit pattern into the sum.
fn scan(value: &simd_json::OwnedValue) -> u64 {
    let mut acc: u64 = 0;
    walk(value, &mut acc);
    acc
}

fn walk(value: &simd_json::OwnedValue, acc: &mut u64) {
    if let Some(object) = value.as_object() {
        for member in object.values() {
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
        let elapsed = common::bench_scan(&value, repeats, scan);
        common::print_result("simd-json", "scan", input.len(), repeats, elapsed);
    }
}
