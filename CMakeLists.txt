cmake_minimum_required(VERSION 4.2)
project(practice_Kaleidoscope)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)


add_executable(practice_Kaleidoscope main.cpp)
target_include_directories(practice_Kaleidoscope PRIVATE ${LLVM_INCLUDE_DIRS})


if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()
target_link_libraries(practice_Kaleidoscope PRIVATE
        LLVM
)