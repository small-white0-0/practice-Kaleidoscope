cmake_minimum_required(VERSION 4.2)
project(practice_Kaleidoscope)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

add_executable(practice_Kaleidoscope
        src/main.cpp
        src/lexer/input_char_stream.cpp
        src/lexer/token_stream.cpp
        src/parser/parser.cpp
        src/codegen/context.cpp
        src/codegen/codegen.cpp
)
target_sources(practice_Kaleidoscope PRIVATE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
        src/lexer/char_stream.hpp
        src/lexer/input_char_stream.hpp
        src/lexer/token.hpp
        src/lexer/token_stream.hpp
        src/common.hpp
        src/parser/ast.hpp
        src/parser/parser.hpp
        src/codegen/context.hpp
)
target_include_directories(practice_Kaleidoscope PRIVATE ${LLVM_INCLUDE_DIRS} src)


if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()
target_link_libraries(practice_Kaleidoscope PRIVATE
        LLVM
)