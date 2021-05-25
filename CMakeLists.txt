cmake_minimum_required(VERSION 3.19)
project(Tutorial VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Optional components
set(JERRY_CMDLINE           ON  CACHE BOOL "Build jerry command line tool?")

configure_file(TutorialConfig.h.in TutorialConfig.h)

add_executable(Tutorial tutorial.cxx)

target_include_directories(Tutorial PUBLIC
        "${PROJECT_BINARY_DIR}")

add_subdirectory(qjs-core)
add_subdirectory(qjs-main)
add_subdirectory(qjs-port/default)

add_subdirectory(tests/unit-core)


