cmake_minimum_required(VERSION 3.22)

project(Scintilla
    VERSION 0.1.0
    DESCRIPTION "Virtual interactive 3D LED array designer"
    LANGUAGES C CXX     # C needed so third_party/kissfft/kiss_fft.c compiles (BUG-010)
)

# ── C++20 ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt automoc/rcc ────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Emit compile_commands.json so clangd / IDE tooling can resolve Qt includes.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
)

# PortAudio (DEC-009) — discovered via pkg-config since there's no upstream
# FindPortAudio.cmake module. See IMP-007 for the CMake-style deviation from
# the D-002 illustrative example.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)

# KissFFT (DEC-010) — vendored under third_party/kissfft/, BSD-3-Clause.
# AUTOMOC off since the vendored C source has no Qt types and shouldn't be scanned.
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)
target_compile_definitions(kissfft PUBLIC kiss_fft_scalar=float)
set_target_properties(kissfft PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

# ── Sources (Phase 1 + Phase 2 + Phase 3 — renderer + editor UI + audio) ─────
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
    src/ui/SliceControlWidget.cpp
    src/ui/TimelineWidget.cpp
    src/ui/FrameInfoPanel.cpp
    src/ui/AudioReactivePanel.cpp

    src/audio/AudioWorker.cpp
    src/audio/FFTProcessor.cpp
    src/audio/AudioReactiveEngine.cpp
    src/audio/AudioDevicePicker.cpp

    src/scripting/PresetRunner.cpp

    src/util/CrashHandler.cpp
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
    src/ui/SliceControlWidget.h
    src/ui/TimelineWidget.h
    src/ui/FrameInfoPanel.h
    src/ui/AudioReactivePanel.h

    src/audio/AudioWorker.h
    src/audio/FFTProcessor.h
    src/audio/AudioReactiveEngine.h
    src/audio/AudioDevicePicker.h

    src/scripting/PresetRunner.h

    src/util/CrashHandler.h
)

# Phase 4 — uncomment when implementing the preset scripting bridge.
# list(APPEND SOURCES
#     src/scripting/PresetRunner.cpp
#     src/scripting/PresetEditorPanel.cpp
# )
# list(APPEND HEADERS
#     src/scripting/PresetRunner.h
#     src/scripting/PresetEditorPanel.h
# )

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(Scintilla ${SOURCES} ${HEADERS})

# ── Shader resources (embedded via Qt resource system) ────────────────────────
qt_add_resources(Scintilla "shaders"
    PREFIX "/shaders"
    BASE "src/renderer/shaders"
    FILES
        src/renderer/shaders/led.vert
        src/renderer/shaders/led.frag
        src/renderer/shaders/ghost.vert
        src/renderer/shaders/ghost.frag
        src/renderer/shaders/grid.vert
        src/renderer/shaders/grid.frag
        src/renderer/shaders/axis.vert
        src/renderer/shaders/axis.frag
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(Scintilla PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    PkgConfig::PORTAUDIO
    kissfft
)

target_include_directories(Scintilla PRIVATE src)

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(Scintilla PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

# Export executable symbols so the CrashHandler's backtrace_symbols_fd can
# resolve them in stack traces (otherwise only library symbols are visible).
target_link_options(Scintilla PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>: -rdynamic>
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
