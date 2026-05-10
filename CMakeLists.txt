cmake_minimum_required(VERSION 3.16)
project(HexapodSim CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HEXAPOD_ENABLE_GUI "Build the raylib window, renderer, and local input path" OFF)

# ============================================================
#  Optional GUI dependency via FetchContent (no install required)
# ============================================================
if(HEXAPOD_ENABLE_GUI)
    include(FetchContent)

    # ---- raylib (window, input, 3-D rendering) ------------------
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    TRUE
    )
    set(BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES     OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# ============================================================
#  Executable
# ============================================================
set(HEXAPOD_SOURCES
    src/control.cpp
    src/main.cpp
    src/mdns_responder.cpp
    src/options.cpp
    src/servo.cpp
    src/wifi_controller.cpp
)

if(HEXAPOD_ENABLE_GUI)
    list(APPEND HEXAPOD_SOURCES
        src/input.cpp
        src/visual.cpp
    )
else()
    list(APPEND HEXAPOD_SOURCES
        src/input_headless.cpp
    )
endif()

add_executable(hexapod_sim ${HEXAPOD_SOURCES})

target_include_directories(hexapod_sim PRIVATE
    src/
)

if(HEXAPOD_ENABLE_GUI)
    target_include_directories(hexapod_sim PRIVATE
        ${raylib_SOURCE_DIR}/src/external/glfw/include
    )
    target_link_libraries(hexapod_sim PRIVATE raylib)
    target_compile_definitions(hexapod_sim PRIVATE HEXAPOD_ENABLE_GUI=1)

    if(TARGET glfw)
        target_link_libraries(hexapod_sim PRIVATE glfw)
    endif()
else()
    target_compile_definitions(hexapod_sim PRIVATE HEXAPOD_HEADLESS=1)
endif()

# ============================================================
#  Platform-specific settings
# ============================================================

# macOS: silence OpenGL deprecation warnings (OpenGL is still
# supported, just deprecated in favour of Metal since 10.14).
if(APPLE AND HEXAPOD_ENABLE_GUI)
    target_compile_definitions(hexapod_sim PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(hexapod_sim PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

# Windows: raylib links against winmm, gdi32 etc. automatically;
# nothing extra needed here.
if(WIN32)
    target_link_libraries(hexapod_sim PRIVATE ws2_32 iphlpapi)
    # Suppress console window for Release builds (optional)
    if(HEXAPOD_ENABLE_GUI AND CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(hexapod_sim PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# Linux: raylib needs these
if(UNIX AND NOT APPLE)
    target_link_libraries(hexapod_sim PRIVATE
        dl pthread m
    )
endif()

# ============================================================
#  Compiler flags — optimisation for simulation speed
# ============================================================
if(MSVC)
    target_compile_options(hexapod_sim PRIVATE /O2 /W3)
else()
    target_compile_options(hexapod_sim PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================
#  Install
# ============================================================
install(TARGETS hexapod_sim DESTINATION bin)
