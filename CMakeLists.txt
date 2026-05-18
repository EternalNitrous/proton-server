cmake_minimum_required(VERSION 3.16)
project(ProtonServer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES     OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(proton-server
    src/config.cpp
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

if(APPLE)
    target_compile_definitions(proton-server PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(proton-server PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

if(WIN32)
    target_link_libraries(proton-server PRIVATE ws2_32 iphlpapi)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(proton-server PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(proton-server PRIVATE
        dl pthread m
    )
endif()

if(MSVC)
    target_compile_options(proton-server PRIVATE /O2 /W3)
else()
    target_compile_options(proton-server PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
endif()

install(TARGETS proton-server DESTINATION bin)
