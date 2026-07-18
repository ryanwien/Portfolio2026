cmake_minimum_required(VERSION 3.16)
project(file_organizer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(organizer_lib src/organizer.cpp)
target_include_directories(organizer_lib PUBLIC src)

add_executable(organize src/main.cpp)
target_link_libraries(organize PRIVATE organizer_lib)

add_executable(organizer_tests tests/test_organizer.cpp)
target_link_libraries(organizer_tests PRIVATE organizer_lib)

enable_testing()
add_test(NAME organizer_tests COMMAND organizer_tests)
