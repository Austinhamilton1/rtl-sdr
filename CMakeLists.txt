cmake_minimum_required(VERSION 3.15)
project(RTL-SDR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.4.0
)

FetchContent_MakeAvailable(Catch2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED IMPORTED_TARGET librtlsdr)

add_subdirectory(dsp)
add_subdirectory(radio)
add_subdirectory(tests)