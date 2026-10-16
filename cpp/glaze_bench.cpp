#include "bench.hpp"

#include <glaze/glaze.hpp>

#include <bit>

namespace {

/// Visits every value in the tree once, folding a bit pattern into `acc`.
std::uint64_t scan_value(const glz::generic &value) {
    std::uint64_t acc = 0;
    if (const auto *object = value.get_if<glz::generic::object_t>()) {
        for (const auto &entry : *object) acc += scan_value(entry.second);
    } else if (const auto *array = value.get_if<glz::generic::array_t>()) {
        for (const auto &item : *array) acc += scan_value(item);
    } else if (const auto *number = value.get_if<double>()) {
        acc += std::bit_cast<std::uint64_t>(*number);
    } else if (const auto *text = value.get_if<std::string>()) {
        acc += text->size();
    } else {
        acc += 1;
    }
    return acc;
}

} // namespace

int main() {
    std::cout << "Glaze benchmark\n";
    std::cout << "data: data/json, input read and cleanup excluded\n";

    for (const auto name : bench::datasets) {
        const auto input = bench::read_file(name);
        const auto repeats = bench::repeat_count(input.size());
        bench::print_dataset(name, input.size(), repeats);

        {
            glz::generic warmup;
            if (glz::read_json(warmup, input)) return 1;
        }
        std::uint64_t elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            glz::generic value;
            const auto error = glz::read_json(value, input);
            bench::black_box(value);
            const auto end = bench::now_ns();
            if (error) return 1;
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("Glaze", "arbitrary-decode", input.size(), repeats, elapsed);

        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            glz::generic value;
            if (glz::read_json(value, input)) return 1;
            const auto output = value.dump();
            const auto end = bench::now_ns();
            if (!output) return 1;
            bench::black_box(*output);
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("Glaze", "transform", input.size(), repeats, elapsed);

        glz::generic document;
        if (glz::read_json(document, input)) return 1;

        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            const auto acc = scan_value(document);
            const auto end = bench::now_ns();
            bench::black_box(acc);
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("Glaze", "scan", input.size(), repeats, elapsed);
    }
}
