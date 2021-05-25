cmake_minimum_required(VERSION 3.19)

project (qjs-main C)

macro(jerry_create_executable JERRY_NAME)
    add_executable(${JERRY_NAME} ${ARGN})
    set_property(TARGET ${JERRY_NAME}
            PROPERTY LINK_FLAGS "${LINKER_FLAGS_COMMON}")

    target_link_libraries(${JERRY_NAME} qjs-core)

    install(TARGETS ${JERRY_NAME} DESTINATION bin)
endmacro()

# Jerry standalones
if(JERRY_CMDLINE)
    jerry_create_executable("qjs" "main-qjs.c")
endif()