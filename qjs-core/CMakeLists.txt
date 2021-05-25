cmake_minimum_required(VERSION 3.19)

set(QJS_CORE_NAME qjs-core)
project (${QJS_CORE_NAME} C)

include(CheckLibraryExists)

add_compile_definitions(CONFIG_VERSION="20210524")

# Include directories
set(INCLUDE_CORE_PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(INCLUDE_CORE_PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/context"
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
        "${CMAKE_CURRENT_SOURCE_DIR}/utils"
        "${CMAKE_CURRENT_SOURCE_DIR}/memory"
        "${CMAKE_CURRENT_SOURCE_DIR}/api")

set(INCLUDE_CORE_PUBLIC ${INCLUDE_CORE_PUBLIC} PARENT_SCOPE) # for qjs-port
set(INCLUDE_CORE_PRIVATE ${INCLUDE_CORE_PRIVATE} PARENT_SCOPE) # for tests/unit-core

set(SOURCE_CORE_FILES
        runtime/qjs-runtime.c
        context/context.c
        utils/cutils.c
        memory/gc.c
        memory/jmemory.c)


add_library(${QJS_CORE_NAME} ${SOURCE_CORE_FILES})

target_include_directories(${QJS_CORE_NAME} PUBLIC ${INCLUDE_CORE_PUBLIC})
target_include_directories(${QJS_CORE_NAME} PRIVATE ${INCLUDE_CORE_PRIVATE})

