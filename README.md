<div align="center" style="display: flex; justify-content: center; align-items: center; gap: 16px;">
  <img src="assets/fullpreview.png" height="125" />
</div>

<div align="center">
  <img src="https://img.shields.io/github/issues/EternalNitrous/proton-server?style=flat&color=2c2c2c" />
  <img src="https://img.shields.io/github/stars/EternalNitrous/proton-server?style=flat&color=2c2c2c" />
</div>



# Proton Server

Interactive hexapod simulation and control runtime with keyboard/gamepad input,
Wi-Fi control, Servo2040 serial output, gait generation, IK/FK, and telemetry
endpoints.

## What's included

| C++ file | Purpose |
|---|---|
| `proton.conf` | User-facing robot profile: dimensions, servos, and Servo2040 pin map |
| `src/config.h` / `src/config.cpp` | Built-in defaults, profile parsing, and config validation |
| `src/robot_params.h` | Robot geometry and default stance parameters |
| `src/kinematics.h` | IK solver and FK helpers |
| `src/gaits.h` | Gait phase tables and foot trajectory engine |
| `src/control.cpp` | Per-frame control updates, gait switching, direct PWM mode |
| `src/input.cpp` | Gamepad / joystick / keyboard input plumbing |
| `src/visual.cpp` | 3-D drawing, footprints, HUD panels, controls legend |
| `src/servo.cpp` | Servo angle/PWM conversion and Servo2040 serial output |
| `src/wifi_controller.cpp` | Built-in Wi-Fi controller HTTP server |
| `src/options.cpp` | Command-line option parsing |
| `src/main.cpp` | Thin setup and per-frame orchestration |

## Controls

Keyboard, controller, and Wi-Fi input switch actively while the simulator is
running. The active source follows whichever method you are currently using;
when a new source starts sending control input, it takes over immediately.

## Robot profile

Most builders should adapt the robot by editing `proton.conf`, not C++ code.
The file uses `key = value` lines, millimetres for keys ending in `_mm`, and
degrees for keys ending in `_deg`. It covers link lengths, body size, leg mount
positions, standing/sitting heights, servo range, PWM centers, solver safety
limits, and the Servo2040 pin map.

The body layout uses measurements a builder can take directly between coxa
rotation centers: `l1_to_r1_mm`, `l1_to_l3_mm`, and `l2_to_r2_mm`.
The server derives the coxa pivot positions from those values while keeping the
visual body outline as a separate chassis shape. `coxa_offsets_deg`,
`footprint_design_knee_angle_deg`, and `footprint_design_height_mm` reproduce
the original simulator's default foot positions.

The app-facing ranges for body height, stance/body radius, step height, and
speed are also configured there. `standing_height_mm` is the live neutral body
height. `body_radius_mm` is measured from the body center to the middle feet;
corner foot distance follows from the coxa offsets.

Validate the profile without opening the simulator:

```bash
./build/proton-server --validate-config --config proton.conf
```

If `proton.conf` exists in the working directory, it is loaded automatically.
Use `--config other.conf` to require a specific file. The default profile is
set up for 270 degree servos, but `servo_type_deg` can be changed if your
hardware uses a different servo range.

### Wi-Fi controller

The simulator starts a Wi-Fi controller server on port 8080 by default:

```bash
./build/proton-server
```

The simulator also advertises itself on the local network with embedded
mDNS/DNS-SD as `_hexapod._tcp`, so the Proton app can fill in the server
address automatically.

`http://<computer-ip>:8080/` returns the current controller status as JSON.
Send joystick updates over a WebSocket at `ws://<computer-ip>:8080/control/ws`
using this payload shape:

```json
{"primary":{"x":0.0,"y":1.0},"secondary":{"x":0.0,"y":0.0},"primary_x":"spin","primary_y":"march","secondary_x":"strafe","secondary_y":"none","relay_status":1,"body_height":0.056,"body_radius":0.15,"step_height":0.08,"speed":0.15,"position":1,"gait":1,"voltage":0.0,"current":0.0}
```

The four axis mapping fields persist across shutdown/startup cycles. Valid
values are `march`, `strafe`, `turn`/`spin`, `height`, `fwbk`, `side`,
`circle`, and `none`. Defaults are `primary_x:"spin"`, `primary_y:"march"`,
`secondary_x:"strafe"`, and `secondary_y:"none"`. Dance mappings use the axis as
signed dance speed, so multiple dance axes can be active at once.

`body_height` sets the requested body height and is clamped to the safe height
range. The legacy `height` key is still accepted as an alias. `body_radius`
moves the neutral body-center-to-foot stance radius, `step_height` sets the gait
foot lift height, and `speed` sets target linear speed. These values use metres
over the wire, so the app can display them as centimetres.
`position:0` moves into the side-parallel sit pose while keeping the relay on;
`position:1` starts the stand/startup animation from that sit pose.
`relay_status` accepts `1`/`0` or `true`/`false`; requesting `0` lowers the body
to sit height and turns the relay off after the robot is low, while requesting
`1` turns the relay on and returns to normal height. Reported `relay_status` and
`position` values change only after the relay or robot has actually reached that
state. Relay shutdown/startup does not change the reported `position`; only an
explicit `position` request does. Directional stick input is ignored while the
relay is off. After 15 seconds with no robot-control activity, the simulator
requests `relay_status:0`. `position`, `gait`, `voltage`, and `current` are
tracked and reported in status JSON.

| Control | Action |
|---|---|
| Primary X mapping | Default `spin`; also accepts `march`, `strafe`, `height`, `fwbk`, `side`, `circle`, `none` |
| Primary Y mapping | Default `march`; also accepts `spin`, `strafe`, `height`, `fwbk`, `side`, `circle`, `none` |
| Secondary X mapping | Default `strafe`; also accepts `march`, `spin`, `height`, `fwbk`, `side`, `circle`, `none` |
| Secondary Y mapping | Default `none`; also accepts `march`, `strafe`, `spin`, `height`, `fwbk`, `side`, `circle` |
| Height | Set requested body height |
| Relay status | `1` relay on and normal height, `0` lower to sit then relay off |
| Position | `0` full shutdown animation, `1` stand/startup animation after shutdown completes |
| Gait | `1` Tripod, `2` RippleExt, `3` Ripple, `4` Amble |
| Voltage / current | Report electrical telemetry; Servo2040 voltage is mirrored when available |
| `{x: 0, y: 0}` for both sticks | Stop Wi-Fi control |

To use a different port:

```bash
./build/proton-server --port 8081
```

### Gamepad

| Control | Action |
|---|---|
| Left stick up / down | March forward / backward |
| Left stick left / right | Turn left / right |
| Right stick left / right | Strafe left / right |
| LB / RB | Lower / raise body height |
| LT / RT | Decrease / increase stored move speed |
| Left stick click / L3 | Creep mode |
| D-pad up | Dance sway front / back |
| D-pad left | Dance sway side / side |
| D-pad right | Dance circular body tilt |
| A / Cross | Cycle through gaits |
| X / Square | Select Tripod gait |
| Y / Triangle | Select Ripple gait |
| B / Circle | Select Amble gait |

### Keyboard

| Key | Action |
|---|---|
| W / S | March forward / backward |
| A / D | Strafe left / right |
| Q / E | Spin CCW / CW in place |
| R / F | Raise / lower body height |
| Up arrow | Dance sway front / back |
| Left arrow | Dance sway side / side |
| Right arrow | Dance circular body tilt |
| Shift | Creep mode (half speed) |
| 1 / 2 / 3 / 4 | Select gait (Tripod / Ripple / Amble / RippleExt) |
| Tab | Cycle through gaits |
| Right Mouse Drag | Orbit camera |
| Scroll Wheel | Zoom |
| Escape | Quit |

Keys are fully combinable — W+A walks diagonally, W+E arcs like a car-turn, etc.

## Build requirements

- CMake ≥ 3.16
- C++17 compiler (GCC, Clang, MSVC)
- Internet connection on first build

raylib is fetched and built automatically — no manual install required.

## Build instructions

### Raspberry Pi OS / Raspbian

Use the installer script from the repo root. It installs apt dependencies,
checks out the selected branch, builds, and enables a `proton-server.service`
startup service:

```bash
sh scripts/install_raspbian.sh headless --servo2040 /dev/ttyACM0
```

Use `headless` for Raspberry Pi OS Lite. Use `main` on Raspberry Pi OS Desktop
when you want the local raylib window:

```bash
sh scripts/install_raspbian.sh main
```

### Linux / macOS

```bash
# Prerequisites: cmake, a C++ compiler, git
# Ubuntu/Debian also needs the raylib window/audio/input deps:
# xorg-dev libasound2-dev mesa-common-dev libgl1-mesa-dev
# libglu1-mesa-dev libudev-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./proton-server
```

### Windows (Visual Studio)

```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
Release\proton-server.exe
```

### Windows (MinGW / MSYS2)

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
proton-server.exe
```

### Cross-compile Windows with MinGW-w64

```bash
cmake -S . -B build-mingw -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-mingw -j4
```

### macOS (Homebrew)

```bash
brew install cmake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
./proton-server
```

## Servo2040 output

To stream the PWM values shown in the left HUD to a Servo2040 board, pass the
serial port when launching the executable:

```bash
./build/proton-server --servo2040 /dev/cu.usbmodemXXXX
```

Windows example:

```bat
build\Release\proton-server.exe --servo2040 COM3
```

The simulator opens the port at 115200 baud, enables relay pin 26 before
streaming, first sends all 18 servo pins to 1500 us, sends all 18 servo pins
in one `SET` command each frame, and turns the relay off when the program
exits. The `--servo2040-port PORT` flag is also accepted.

While connected, the simulator reads Servo2040 current pin 24 and voltage pin
25 every 0.5 seconds. Voltage shutdown is armed only after the relay has been
on for 3 seconds; low voltage while the relay is off is ignored. Once armed,
below 7.0 V the background pulses yellow every 5 seconds. Two consecutive
readings below 6.0 V turn the background red, request the walking shutdown
sequence, and turn the relay off when shutdown completes.

For hardware-free debugging, run with `--servo2040-pwm-sim` or `--pwm-sim`.
This renders the simulated robot from the same Servo2040 PWM packet that would
be sent over serial, including the pin mapping.
Direction flips for the physical hexapod are applied only to the serial output.

To skip gait and IK entirely and directly pose the simulated robot with PWM
values, run:

```bash
./build/proton-server --pwm-control
```

To pipe those manually controlled PWM values to the real Servo2040 at the same
time, pass a serial port too:

```bash
./build/proton-server --pwm-control --servo2040 /dev/cu.usbmodemXXXX
```

Or use the convenience flag:

```bash
./build/proton-server --pwm-control-servo2040 /dev/cu.usbmodemXXXX
```

Direct PWM mode starts every servo at 1500 us. With a gamepad connected, use
D-pad left/right to select a leg, D-pad up/down to select coxa/femur/tibia,
`A`/`B` to change PWM, left stick click / `L3` for 50 us steps, `X` to reset the selected PWM to
1500, and `Y` to restore the all-1500 startup pose. Without a gamepad, use
`1`-`6` or left/right arrows to select a leg, `Z`/`X` or `Tab` to select a
joint, and up/down arrows or `+`/`-` to change PWM. Hold `Ctrl` for 1 us steps
or `Shift` for 50 us steps. `R` resets the selected PWM to 1500 and `C`
restores the all-1500 startup pose.

## Robot specifications

- **Body**: configured from coxa-center measurements in `proton.conf`
- **Legs**: 6 × 3-DOF (coxa–femur–tibia, no hip joint)
  - Coxa: 43 mm
  - Femur: 80 mm
  - Tibia: 134 mm
- **Gait modes**:
  - **Tripod** — alternating 3-leg groups (fastest, stable at speed)
  - **Ripple** — table-driven wave gait
  - **Amble** — staggered quarter-cycle gait
  - **RippleExt** — extended ripple gait with longer swing windows

## Architecture

The core runtime is split into a few focused pieces:

- **IK**: World → body frame (ZYX Euler), body → leg-local frame, 2-link sagittal plane solver
- **FK**: Standard 4×4 homogeneous transform chain (Rz–Ry–Rx, column-major)
- **Gaits**: Table-driven phase windows, phase-locked mode switching, strain-triggered realignment, stride safety, Raibert foot placement, workspace clamping, and body sway

The FK error (yellow lines, if any) shown in the HUD should be ≤ 0.1 mm under normal operation.

This repo was heavily based on waw's MATLAB simulation, which was based on Make Your Pet's Chica Server app.
