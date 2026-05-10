cmake_minimum_required(VERSION 3.16)
project(ProtonServer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
#  Executable
# ============================================================
add_executable(proton-server
    src/control.cpp
    src/input_headless.cpp
    src/main.cpp
    src/mdns_responder.cpp
    src/options.cpp
    src/servo.cpp
    src/wifi_controller.cpp
)

target_include_directories(proton-server PRIVATE
    src/
)

# ============================================================
#  Platform-specific settings
# ============================================================

if(WIN32)
    target_link_libraries(proton-server PRIVATE ws2_32 iphlpapi)
endif()

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
