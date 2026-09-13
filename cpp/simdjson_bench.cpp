#include "bench.hpp"

#include <simdjson.h>

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

        simdjson::padded_string input{source};
        simdjson::dom::parser parser;
        auto document = parser.parse(input);
        if (document.error()) return 1;
        const auto root = document.value_unsafe();
        const auto path = bench::get_path(name);

        {
            auto warmup = root.at_pointer(path);
            if (warmup.error()) return 1;
            bench::black_box(warmup.value_unsafe());
        }
        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            for (std::size_t b = 0; b < bench::get_batch; ++b) {
                auto found = root.at_pointer(path);
                bench::black_box(found.value_unsafe());
            }
            const auto end = bench::now_ns();
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("simdjson", "get", source.size(), repeats * bench::get_batch, elapsed);
    }
}
