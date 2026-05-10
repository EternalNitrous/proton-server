# Hexapod Kinematic Simulator

Headless hexapod simulation and control runtime with Wi-Fi control,
Servo2040 serial output, gait generation, IK/FK, and telemetry endpoints.

## What's included

| C++ file | Purpose |
|---|---|
| `src/config.h` | User-tunable robot, gait, servo, and hardware settings |
| `src/robot_params.h` | Robot geometry and default stance parameters |
| `src/kinematics.h` | IK solver and FK helpers |
| `src/gaits.h` | Gait phase tables and foot trajectory engine |
| `src/control.cpp` | Per-frame control updates, gait switching, direct PWM mode |
| `src/input_headless.cpp` | Headless no-local-input stub for Wi-Fi / Servo2040 operation |
| `src/servo.cpp` | Servo angle/PWM conversion and Servo2040 serial output |
| `src/wifi_controller.cpp` | Built-in Wi-Fi controller HTTP server |
| `src/options.cpp` | Command-line option parsing |
| `src/main.cpp` | Thin setup and per-frame orchestration |

## Controls

This branch is headless: it runs the simulation, Wi-Fi controller, Servo2040
output, gait, IK, and websocket state updates without opening a local window or
reading local keyboard/gamepad input.

### Wi-Fi controller

The simulator starts a Wi-Fi controller server on port 8080 by default:

```bash
./build/hexapod_sim
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
moves the neutral stance radius, `step_height` sets the gait foot lift height,
and `speed` sets target linear speed. These values use metres over the wire, so
the app can display them as centimetres.
`position:0` runs the full shutdown animation without closing the simulator;
`position:1` starts the stand/startup animation only after shutdown has
completed. `relay_status` accepts `1`/`0` or `true`/`false`; requesting `0`
lowers to sit height and turns the relay off after the robot is low, while
requesting `1` turns the relay on and returns to normal height. Reported
`relay_status` and `position` values change only after the relay or robot has
actually reached that state. Directional stick input is ignored while the relay
is off. After 15 seconds with no robot-control activity, the simulator requests
`relay_status:0`. `position`, `gait`, `voltage`, and `current` are tracked and
reported in status JSON.

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
./build/hexapod_sim --port 8081
```

## Build requirements

- CMake ≥ 3.16
- C++17 compiler (GCC, Clang, MSVC)

## Build instructions

### Linux / macOS

```bash
# Prerequisites: cmake and a C++ compiler

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./hexapod_sim
```

### Windows (Visual Studio)

```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
Release\hexapod_sim.exe
```

### Windows (MinGW / MSYS2)

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
hexapod_sim.exe
```

### macOS (Homebrew)

```bash
brew install cmake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
./hexapod_sim
```

## Servo2040 output

To stream generated PWM values to a Servo2040 board, pass the serial port when
launching the executable:

```bash
./build/hexapod_sim --servo2040 /dev/cu.usbmodemXXXX
```

Windows example:

```bat
build\Release\hexapod_sim.exe --servo2040 COM3
```

The simulator opens the port at 115200 baud, enables relay pin 26 before
streaming, first sends all 18 servo pins to 1500 us, sends all 18 servo pins
in one `SET` command each frame, and turns the relay off when the program
exits. The `--servo2040-port PORT` flag is also accepted.

While connected, the simulator reads Servo2040 current pin 24 and voltage pin
25 every 0.5 seconds. Voltage shutdown is armed only after the relay has been
on for 3 seconds; low voltage while the relay is off is ignored. Once armed,
two consecutive readings below 6.0 V request the walking shutdown sequence and
turn the relay off when shutdown completes.

For hardware-free debugging, run with `--servo2040-pwm-sim` or `--pwm-sim`.
This mirrors the same Servo2040 PWM packet that would be sent over serial,
including the pin mapping.
Direction flips for the physical hexapod are applied only to the serial output.

To skip gait and IK entirely and directly pose the simulated robot with PWM
values, run:

```bash
./build/hexapod_sim --pwm-control
```

To pipe those manually controlled PWM values to the real Servo2040 at the same
time, pass a serial port too:

```bash
./build/hexapod_sim --pwm-control --servo2040 /dev/cu.usbmodemXXXX
```

Or use the convenience flag:

```bash
./build/hexapod_sim --pwm-control-servo2040 /dev/cu.usbmodemXXXX
```

Direct PWM mode starts every servo at 1500 us. In this headless branch there is
no local keyboard/gamepad PWM editor; use it with Servo2040 output or extend the
Wi-Fi protocol if manual remote PWM editing is needed.

## Robot specifications

Most robot-specific values are in `src/config.h`. Edit that file when adapting
the simulator to a different hexapod: link lengths, body dimensions, leg mount
angles/radii, default height, gait tuning, servo calibration, PWM limits,
Servo2040 pin order, and hardware direction flips all live there.

- **Body**: 140 × 100 mm rectangle with 20 mm 45-degree chamfered corners
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

FK error should be <= 0.1 mm under normal operation.

This repo was heavily based on waw's MATLAB simulation, which was based on Make Your Pet's Chica Server app.
