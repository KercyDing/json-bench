cmake_minimum_required(VERSION 3.24)
project(json_bench LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    simdjson
    URL https://github.com/simdjson/simdjson/archive/refs/tags/v4.6.11.tar.gz
)
FetchContent_Declare(
    glaze
    URL https://github.com/stephenberry/glaze/archive/refs/tags/v8.3.0.tar.gz
)
FetchContent_MakeAvailable(simdjson glaze)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(yyjson_bench c/yyjson_bench.c c/yyjson/yyjson.c)
target_include_directories(yyjson_bench PRIVATE c/yyjson)
target_link_libraries(yyjson_bench PRIVATE m)

add_executable(simdjson_bench cpp/simdjson_bench.cpp)
target_include_directories(simdjson_bench PRIVATE cpp)
target_link_libraries(simdjson_bench PRIVATE simdjson)

add_executable(glaze_bench cpp/glaze_bench.cpp)
target_include_directories(glaze_bench PRIVATE cpp ${glaze_SOURCE_DIR}/include)

foreach(target yyjson_bench simdjson_bench glaze_bench)
    target_compile_definitions(${target} PRIVATE BENCH_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/json")
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-O3;-march=native>)
endforeach()
