cmake_minimum_required(VERSION 3.21)
project(ifmqoi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static CRT: the plugin is dropped into a host's folder as-is, so it must not
# depend on a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ifmqoi requires a 64-bit (x64) build. Pass -A x64.")
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_library(ifmqoi SHARED
    src/ifmqoi.cpp
    src/qoi_decoder.cpp
)
target_include_directories(ifmqoi PRIVATE include)
set_target_properties(ifmqoi PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ifmqoi"
    SUFFIX ".sph"
)
if(MSVC)
    set_target_properties(ifmqoi PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/ifmqoi.def"
    )
endif()

option(IFMQOI_BUILD_TESTS "Build smoke tests" ON)
if(IFMQOI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
