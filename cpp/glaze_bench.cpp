#include "bench.hpp"

#include "../c/bench_paths.h"

#include <glaze/glaze.hpp>

namespace {

const bench_path *find_path(std::string_view name) {
    for (const auto &path : bench_paths) {
        if (name == path.dataset) return &path;
    }
    return nullptr;
}

/// Walks the precompiled path with Glaze's generic object/array accessors.
const glz::generic *get_target(const glz::generic &root, const bench_path &path) {
    const glz::generic *current = &root;
    for (std::size_t i = 0; i < path.count; i++) {
        const bench_step &step = path.steps[i];
        if (step.is_index) {
            const auto *array = current->get_if<glz::generic::array_t>();
            if (array == nullptr || step.index >= array->size()) return nullptr;
            current = &(*array)[step.index];
        } else {
            const auto *object = current->get_if<glz::generic::object_t>();
            if (object == nullptr) return nullptr;
            const auto it = object->find(std::string_view(step.field));
            if (it == object->end()) return nullptr;
            current = &it->second;
        }
    }
    return current;
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

        const auto *path = find_path(name);
        glz::generic document;
        if (glz::read_json(document, input) || path == nullptr) return 1;
        bench::black_box(get_target(document, *path));

        elapsed = 0;
        for (std::size_t i = 0; i < repeats; ++i) {
            const auto start = bench::now_ns();
            for (std::size_t b = 0; b < bench::get_batch; ++b) bench::black_box(get_target(document, *path));
            const auto end = bench::now_ns();
            elapsed += std::max<std::uint64_t>(1, end - start);
        }
        bench::print_result("Glaze", "get", input.size(), repeats * bench::get_batch, elapsed);
    }
}
