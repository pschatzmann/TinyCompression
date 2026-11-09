cmake_minimum_required(VERSION 3.13)
project(TinyCompression LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TC_BUILD_TESTS "Build the desktop test suite" ON)
option(TC_BUILD_EXAMPLES "Build the RoundTrip Arduino sketch as a desktop program" ON)
option(TC_SANITIZE "Build tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Header-only interface target: consumers only need the include directory.
# TC_DESKTOP selects the desktop Print/Stream headers (src/tinycompression/desktop/).
add_library(TinyCompression INTERFACE)
target_include_directories(TinyCompression INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(TinyCompression INTERFACE TC_DESKTOP=1)

if(TC_BUILD_EXAMPLES)
  enable_testing()
  add_executable(tinycompression_roundtrip examples/desktop/main.cpp)
  target_link_libraries(tinycompression_roundtrip PRIVATE TinyCompression)
  target_compile_options(tinycompression_roundtrip PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME roundtrip_example COMMAND tinycompression_roundtrip)
  set_tests_properties(roundtrip_example PROPERTIES PASS_REGULAR_EXPRESSION "\nOK\n" FAIL_REGULAR_EXPRESSION "ERROR")
endif()

if(TC_BUILD_TESTS)
  enable_testing()
  add_executable(tinycompression_tests
    test/test_main.cpp
    test/test_codecs.cpp
    test/test_stream.cpp
    test/test_external.cpp)
  target_link_libraries(tinycompression_tests PRIVATE TinyCompression)
  target_include_directories(tinycompression_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
  target_compile_definitions(tinycompression_tests PRIVATE
    TC_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
  target_compile_options(tinycompression_tests PRIVATE -Wall -Wextra -Wpedantic)

  if(TC_SANITIZE)
    target_compile_options(tinycompression_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(tinycompression_tests PRIVATE -fsanitize=address,undefined)
  endif()

  add_test(NAME tinycompression COMMAND tinycompression_tests)
endif()
