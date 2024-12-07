cmake_minimum_required(VERSION 3.10)
project(libspng_test)

# Set the version number
set(SPNG_VERSION "0.1.0")

# Add the library
add_subdirectory(libspng)

# Include directories
include_directories(libspng/spng)

# Set AFL++ as the compiler
set(CMAKE_C_COMPILER afl-clang-fast)
set(CMAKE_CXX_COMPILER afl-clang-fast++)

# Set AFL++ instrumentation flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g")

# Add the executable for the test program
add_executable(generic_test fuzz/generic_test.c)

# Link the library to the executable dynamically
target_link_libraries(generic_test spng_static)