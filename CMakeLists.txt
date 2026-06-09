cmake_minimum_required(VERSION 3.24)

project(Penumbra
    VERSION 0.0.1
    DESCRIPTION "Penumbra — retained-mode C++20 UI framework on SDL3 (PoC)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# The expected path: SDL3 / SDL3_ttf installed with CMake config packages.
find_package(SDL3     CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Penumbra — static library. Nothing in here references demo/.
# ---------------------------------------------------------------------------
add_library(penumbra STATIC
    src/Penumbra/Platform/PlatformWindow.cpp
    src/Penumbra/Render/Renderer.cpp
    src/Penumbra/Render/SdlTtfFontBackend.cpp
    src/Penumbra/Widgets/Box.cpp
    src/Penumbra/Widgets/Button.cpp
    src/Penumbra/Widgets/ScrollablePanel.cpp
    src/Penumbra/Widgets/Label.cpp
    src/Penumbra/Widgets/Checkbox.cpp
    src/Penumbra/Widgets/NumericDrag.cpp
    src/Penumbra/Widgets/TextInput.cpp
    src/Penumbra/Widgets/ViewportWidget.cpp)

target_include_directories(penumbra PUBLIC include)

# SDL3 is PUBLIC so the demo sees SDL types (e.g. SDL_Color) transitively.
# SDL3_ttf is linked now though only used from Milestone 1 onward.
target_link_libraries(penumbra PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf)

# ---------------------------------------------------------------------------
# Demo — executable. The ONLY target that defines values. Links penumbra.
# ---------------------------------------------------------------------------
add_executable(penumbra_demo
    demo/main.cpp
    demo/DemoTheme.cpp
    demo/DemoResolvers.cpp)

target_link_libraries(penumbra_demo PRIVATE penumbra)
set_target_properties(penumbra_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/demo")

# Absolute path to the demo's vendored assets, so the binary finds its font
# regardless of the working directory it is launched from. A demo concern only.
target_compile_definitions(penumbra_demo PRIVATE
    DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/assets")
