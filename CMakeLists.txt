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
FetchContent_Declare(
    yyjson
    URL https://github.com/ibireme/yyjson/archive/refs/tags/0.13.0.tar.gz
    SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(simdjson glaze yyjson)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(yyjson_bench c/yyjson_bench.c ${yyjson_SOURCE_DIR}/src/yyjson.c)
target_include_directories(yyjson_bench PRIVATE ${yyjson_SOURCE_DIR}/src)
target_link_libraries(yyjson_bench PRIVATE m)

add_executable(simdjson_bench cpp/simdjson_bench.cpp)
target_include_directories(simdjson_bench PRIVATE cpp)
target_link_libraries(simdjson_bench PRIVATE simdjson)

add_executable(glaze_bench cpp/glaze_bench.cpp)
target_include_directories(glaze_bench PRIVATE cpp ${glaze_SOURCE_DIR}/include)

foreach(target yyjson_bench simdjson_bench glaze_bench)
    target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-O3;-march=native>)
endforeach()
