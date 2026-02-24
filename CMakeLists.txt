cmake_minimum_required(VERSION 3.20)
project(luatypetest CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(benchmark CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

add_executable(luatypetest src/bench.cpp)
target_link_libraries(luatypetest PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    sol2::sol2
    ${LUA_LIBRARIES})
target_include_directories(luatypetest PRIVATE ${LUA_INCLUDE_DIR})
# Disable sol2 safety checks for fair perf comparison
target_compile_definitions(luatypetest PRIVATE SOL_ALL_SAFETIES_ON=0)
