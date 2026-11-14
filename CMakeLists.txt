cmake_minimum_required(VERSION 3.16)

project(SmartStudyPlanner VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wno-unknown-pragmas)
endif()

add_executable(SmartStudyPlanner src/main.cpp)

# Winsock is required for the built-in HTTP server on Windows
if (WIN32)
    target_link_libraries(SmartStudyPlanner PRIVATE ws2_32)
endif()

# CMake / make builds cannot use `#pragma comment(lib, ...)`
if (NOT MSVC)
    target_compile_definitions(SmartStudyPlanner PRIVATE
        $<$<PLATFORM_ID:Windows>:NO_PRAGMA_COMMENT>)
endif()
