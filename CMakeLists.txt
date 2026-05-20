cmake_minimum_required(VERSION 3.22)

project(Scintilla
    VERSION 0.1.0
    DESCRIPTION "Virtual interactive 3D LED array designer"
    LANGUAGES CXX
)

# ── C++20 ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt automoc/rcc ────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
# PortAudio — system output loopback capture (DEC-009)
find_package(PortAudio REQUIRED)

# KissFFT — vendored, two files, MIT licence (DEC-010)
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp

    src/core/VoxelFrame.cpp
    src/core/ShapeMask.cpp
    src/core/AnimationTimeline.cpp
    src/core/JsonSerializer.cpp

    src/renderer/CubeViewport.cpp
    src/renderer/OrbitCamera.cpp
    src/renderer/LedInstanceBuffer.cpp

    src/ui/ColorPickerWidget.cpp
    src/ui/TimelineWidget.cpp
    src/ui/SliceControlWidget.cpp
    src/ui/FrameInfoPanel.cpp

    src/audio/AudioWorker.cpp
    src/audio/FFTProcessor.cpp
    src/audio/AudioReactiveEngine.cpp
    src/audio/AudioDevicePicker.cpp
)

set(HEADERS
    src/MainWindow.h

    src/core/VoxelFrame.h
    src/core/ShapeMask.h
    src/core/AnimationTimeline.h
    src/core/JsonSerializer.h

    src/renderer/CubeViewport.h
    src/renderer/OrbitCamera.h
    src/renderer/LedInstanceBuffer.h

    src/ui/ColorPickerWidget.h
    src/ui/TimelineWidget.h
    src/ui/SliceControlWidget.h
    src/ui/FrameInfoPanel.h

    src/audio/AudioWorker.h
    src/audio/FFTProcessor.h
    src/audio/AudioReactiveEngine.h
    src/audio/AudioDevicePicker.h
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(Scintilla ${SOURCES} ${HEADERS})

# ── Shader resources (embedded via Qt resource system) ────────────────────────
qt_add_resources(Scintilla "shaders"
    PREFIX "/shaders"
    FILES
        src/renderer/shaders/led.vert
        src/renderer/shaders/led.frag
        src/renderer/shaders/ghost.vert
        src/renderer/shaders/ghost.frag
        src/renderer/shaders/grid.vert
        src/renderer/shaders/grid.frag
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(Scintilla PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    PortAudio::PortAudio
    kissfft
)

target_include_directories(Scintilla PRIVATE src)

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(Scintilla PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS Scintilla
    RUNTIME DESTINATION bin
)

# ── Build info ────────────────────────────────────────────────────────────────
message(STATUS "Scintilla ${PROJECT_VERSION}")
message(STATUS "  Qt6:      ${Qt6_VERSION}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build:    ${CMAKE_BUILD_TYPE}")
