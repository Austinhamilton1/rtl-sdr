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

# -----------------------------------------------------------------
# Dependencies: GLFW3 & OpenGL
# -----------------------------------------------------------------
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

# -----------------------------------------------------------------
# Dependencies: Catch2
# -----------------------------------------------------------------
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# -----------------------------------------------------------------
# Dependencies: Dear ImGui (Using Official Repository Backends)
# -----------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY  https://github.com/ocornut/imgui.git
    GIT_TAG         v1.91.1
)
FetchContent_MakeAvailable(imgui)

# Compile ImGui with the official GLFW and OpenGL3 backends
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Expose both the root folder and the backends folder
target_include_directories(imgui 
PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# Link GLFW and OpenGL directly to our ImGui target layer
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# -----------------------------------------------------------------
# Hardware and System Libraries
# -----------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED IMPORTED_TARGET librtlsdr)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

# -----------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------
add_subdirectory(audio)
add_subdirectory(dsp)
add_subdirectory(radio)
add_subdirectory(tests)
add_subdirectory(backends)
add_subdirectory(apps)