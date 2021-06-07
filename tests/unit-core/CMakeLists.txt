cmake_minimum_required(VERSION 3.19)
project(unittests-core C)


# Unit tests main modules
set(SOURCE_UNIT_TEST_MAIN_MODULES
        test-token.c
        test-scaffold.c
        test-jsstring.c
        test-memory.c)

# Unit tests declaration
add_executable(unittests-core test-token.c)

target_link_libraries(unittests-core qjs-core qjs-port-default)

target_include_directories(unittests-core PRIVATE ${INCLUDE_CORE_PRIVATE})
