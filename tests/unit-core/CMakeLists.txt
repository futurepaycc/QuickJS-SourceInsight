cmake_minimum_required(VERSION 3.19)
project(unit-core C)


# Unit tests main modules
set(SOURCE_UNIT_TEST_MAIN_MODULES
        test-memory.c)

# Unit tests declaration
add_executable(unit-test test-memory.c)

target_link_libraries(unit-test qjs-core qjs-port-default)

