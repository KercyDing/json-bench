#include "bench.hpp"

#include <simdjson.h>

#include <bit>

namespace {

/// Visits every value in the tree once, folding a bit pattern into `acc`.
std::uint64_t scan_value(const simdjson::dom::element &element) {
    std::uint64_t acc = 0;
    switch (element.type()) {
    case simdjson::dom::element_type::OBJECT:
        for (const auto field : element.get_object()) acc += scan_value(field.value);
        break;
    case simdjson::dom::element_type::ARRAY:
        for (const auto child : element.get_array()) acc += scan_value(child);
        break;
    case simdjson::dom::element_type::DOUBLE:
    case simdjson::dom::element_type::INT64:
    case simdjson::dom::element_type::UINT64: {
        double number = 0.0;
        if (element.get_double().get(number)) return acc + 1;
        acc += std::bit_cast<std::uint64_t>(number);
        break;
    }
    case simdjson::dom::element_type::STRING:
        acc += element.get_string().value_unsafe().size();
        break;
    default:
        acc += 1;
        break;
    }
    return acc;
}

} // namespace

int main() {
    std::cout << "simdjson benchmark\n";
    std::cout << "data: data/json, input read and cleanup excluded\n";

    for (const auto name : bench::datasets) {
        const auto source = bench::read_file(name);
        const auto repeats = bench::repeat_count(source.size());
        bench::print_dataset(name, source.size(), repeats);

        {
            simdjson::padded_string input{source};
            simdjson::dom::parser parser;
            auto warmup = parser.parse(input);
            if (warmup.error()) return 1;
            bench::black_box(warmup.value_unsafe());
        }
        std::uint64_t elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            simdjson::padded_string input{source};
            simdjson::dom::parser parser;
            auto document = parser.parse(input);
            bench::black_box(document.value_unsafe());
            const auto end = bench::now_ns();
            if (document.error()) return 1;
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("simdjson", "arbitrary-decode", source.size(), repeats, elapsed);

        {
            simdjson::padded_string input{source};
            simdjson::dom::parser parser;
            auto warmup = parser.parse(input);
            if (warmup.error()) return 1;
            bench::black_box(simdjson::minify(warmup.value_unsafe()));
        }
        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            simdjson::padded_string input{source};
            simdjson::dom::parser parser;
            auto document = parser.parse(input);
            if (document.error()) return 1;
            const auto output = simdjson::minify(document.value_unsafe());
            const auto end = bench::now_ns();
            bench::black_box(output);
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("simdjson", "transform", source.size(), repeats, elapsed);

        simdjson::padded_string input{source};
        simdjson::dom::parser parser;
        auto document = parser.parse(input);
        if (document.error()) return 1;
        const auto root = document.value_unsafe();

        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            const auto acc = scan_value(root);
            const auto end = bench::now_ns();
            bench::black_box(acc);
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("simdjson", "scan", source.size(), repeats, elapsed);
    }
}
