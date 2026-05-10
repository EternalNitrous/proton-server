cmake_minimum_required(VERSION 3.16)
project(ProtonServer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
#  Dependencies via FetchContent (no install required)
# ============================================================
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

# ============================================================
#  Executable
# ============================================================
add_executable(proton-server
    src/control.cpp
    src/input.cpp
    src/main.cpp
    src/mdns_responder.cpp
    src/options.cpp
    src/servo.cpp
    src/visual.cpp
    src/wifi_controller.cpp
)

target_include_directories(proton-server PRIVATE
    src/
    ${raylib_SOURCE_DIR}/src/external/glfw/include
)

target_link_libraries(proton-server PRIVATE
    raylib
)

if(TARGET glfw)
    target_link_libraries(proton-server PRIVATE glfw)
endif()

# ============================================================
#  Platform-specific settings
# ============================================================

# macOS: silence OpenGL deprecation warnings (OpenGL is still
# supported, just deprecated in favour of Metal since 10.14).
if(APPLE)
    target_compile_definitions(proton-server PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(proton-server PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

# Windows: raylib links against winmm, gdi32 etc. automatically;
# nothing extra needed here.
if(WIN32)
    target_link_libraries(proton-server PRIVATE ws2_32 iphlpapi)
    # Suppress console window for Release builds (optional)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(proton-server PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# Linux: raylib needs these
if(UNIX AND NOT APPLE)
    target_link_libraries(proton-server PRIVATE
        dl pthread m
    )
endif()

# ============================================================
#  Compiler flags — optimisation for simulation speed
# ============================================================
if(MSVC)
    target_compile_options(proton-server PRIVATE /O2 /W3)
else()
    target_compile_options(proton-server PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

# ============================================================
#  Install
# ============================================================
install(TARGETS proton-server DESTINATION bin)
