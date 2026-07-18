cmake_minimum_required(VERSION 3.16)
project(house_price_regression CXX)

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

add_library(regression src/regression.cpp)
target_include_directories(regression PUBLIC src)

add_executable(train src/main.cpp)
target_link_libraries(train PRIVATE regression)

add_executable(regression_tests tests/test_regression.cpp)
target_link_libraries(regression_tests PRIVATE regression)

enable_testing()
add_test(NAME regression_tests COMMAND regression_tests)
