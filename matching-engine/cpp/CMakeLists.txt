cmake_minimum_required(VERSION 3.16)
project(matching_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(order_book src/order_book.cpp)
target_include_directories(order_book PUBLIC src)

add_executable(reference_trace src/reference_trace.cpp)
target_link_libraries(reference_trace PRIVATE order_book)

add_executable(engine_tests tests/test_order_book.cpp)
target_link_libraries(engine_tests PRIVATE order_book)

add_executable(benchmark bench/benchmark.cpp)
target_link_libraries(benchmark PRIVATE order_book)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
