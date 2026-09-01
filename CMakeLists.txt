cmake_minimum_required(VERSION 3.16)

project(quark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(quarkc
    src/main.cpp
    src/Token.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Analyzer.cpp
    src/CodeGen.cpp
    src/AstView.cpp
    src/AstRender.cpp
)

target_include_directories(quarkc PRIVATE include)

if(MSVC)
    target_compile_options(quarkc PRIVATE /W4)
else()
    target_compile_options(quarkc PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_test(
    NAME quark_tests
    COMMAND ${CMAKE_COMMAND} -E env QUARKC=$<TARGET_FILE:quarkc>
            ${CMAKE_SOURCE_DIR}/tests/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
