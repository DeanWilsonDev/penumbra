cmake_minimum_required(VERSION 3.30)

project(Penumbra
    VERSION 0.0.1
    DESCRIPTION "Penumbra — retained-mode C++26 UI framework on SDL3"
    LANGUAGES CXX)

if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
   OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19))
  message(FATAL_ERROR "Penumbra needs GCC 14 or later or Clang 19 or later; found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# The expected path: SDL3 / SDL3_ttf / SDL3_image installed with CMake config packages.
find_package(SDL3       CONFIG REQUIRED)
find_package(SDL3_ttf   CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Penumbra — static library. Nothing in here references demo/.
# ---------------------------------------------------------------------------
add_library(penumbra STATIC
    src/Penumbra/Application.cpp
    src/Penumbra/LifecycleRegistry.cpp
    src/Penumbra/Platform/PlatformWindow.cpp
    src/Penumbra/Render/Renderer.cpp
    src/Penumbra/Render/SdlTtfFontBackend.cpp
    src/Penumbra/Render/TextWrap.cpp
    src/Penumbra/Backends/SdlImageBackend.cpp
    src/Penumbra/Widgets/Box.cpp
    src/Penumbra/Widgets/Chart.cpp
    src/Penumbra/Widgets/BarChart.cpp
    src/Penumbra/Widgets/DonutChart.cpp
    src/Penumbra/Widgets/LineChart.cpp
    src/Penumbra/Widgets/CanvasWidget.cpp
    src/Penumbra/Widgets/InlineContainer.cpp
    src/Penumbra/Widgets/ImageWidget.cpp
    src/Penumbra/Widgets/IconWidget.cpp
    src/Penumbra/Widgets/Button.cpp
    src/Penumbra/Widgets/ScrollablePanel.cpp
    src/Penumbra/Widgets/Label.cpp
    src/Penumbra/Widgets/LayoutPass.cpp
    src/Penumbra/Widgets/Length.cpp
    src/Penumbra/Widgets/Checkbox.cpp
    src/Penumbra/Widgets/Chevron.cpp
    src/Penumbra/Widgets/Dropdown.cpp
    src/Penumbra/Widgets/NumericDrag.cpp
    src/Penumbra/Widgets/TextInput.cpp
    src/Penumbra/Widgets/TextArea.cpp
    src/Penumbra/Widgets/TextSelection.cpp
    src/Penumbra/Widgets/ViewportWidget.cpp
    src/Penumbra/Widgets/SplitPanel.cpp
    src/Penumbra/Widgets/OverlayHost.cpp)

target_include_directories(penumbra PUBLIC include)

# SDL3 is linked PUBLIC because a couple of internals (ViewportWidget's scene
# texture, PlatformWindow's SDL_Renderer*) are legitimate exceptions to
# Penumbra's own "no SDL above Platform" rule; the demo does not need an SDL
# include for ordinary widget/color/geometry code (Color/Point/Rect/Key cover it).
# SDL3_ttf is linked now though only used from Milestone 1 onward.
target_link_libraries(penumbra PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf)

# SDL3_image is linked PRIVATE: unlike SDL3/SDL3_ttf, no public Penumbra header
# names an SDL3_image type (IImageBackend/SdlImageBackend/ImageWidget only use
# SDL_Texture/SDL_Renderer from SDL3 core) -- it's an implementation detail
# entirely contained in SdlImageBackend.cpp, so consumers don't need it on their
# own include/link path.
target_link_libraries(penumbra PRIVATE SDL3_image::SDL3_image)

add_subdirectory(icons)

add_executable(penumbra_demo
    demo/main.cpp
    demo/DemoTheme.cpp
    demo/DemoResolvers.cpp)

target_link_libraries(penumbra_demo PRIVATE penumbra penumbra_icons)
set_target_properties(penumbra_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/demo")

# Absolute path to the demo's vendored assets, so the binary finds its font
# regardless of the working directory it is launched from. A demo concern only.
target_compile_definitions(penumbra_demo PRIVATE
    DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/assets")
