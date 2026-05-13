#include "input.h"

#ifndef GLFW_INCLUDE_NONE
    #define GLFW_INCLUDE_NONE
#endif
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int MAX_JOYSTICKS_TO_SCAN = GLFW_JOYSTICK_LAST + 1;

float apply_deadzone(float v, float deadzone)
{
    if (std::fabs(v) <= deadzone) return 0.0f;
    float sign = (v < 0.0f) ? -1.0f : 1.0f;
    return sign * (std::fabs(v) - deadzone) / (1.0f - deadzone);
}

float trigger_pressure(float axis)
{
    float pressure = (axis + 1.0f) * 0.5f;
    if (pressure <= config::GamepadTriggerDeadzone) return 0.0f;
    return (pressure - config::GamepadTriggerDeadzone) / (1.0f - config::GamepadTriggerDeadzone);
}

float raw_trigger_pressure(float axis)
{
    if (axis < -0.05f) return trigger_pressure(axis);
    if (axis <= config::GamepadTriggerDeadzone) return 0.0f;
    return (axis - config::GamepadTriggerDeadzone) / (1.0f - config::GamepadTriggerDeadzone);
}

std::array<int, 4> raw_button_indices_for_gamepad_button(int button)
{
    switch (button) {
        case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:  return {0, -1, -1, -1};  // A / Cross
        case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return {1, -1, -1, -1};  // B / Circle
        case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:  return {2, -1, -1, -1};  // X / Square
        case GAMEPAD_BUTTON_RIGHT_FACE_UP:    return {3, -1, -1, -1};  // Y / Triangle
        case GAMEPAD_BUTTON_LEFT_TRIGGER_1:   return {4, 6, -1, -1};   // LB / L1
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:  return {5, 7, -1, -1};   // RB / R1
        case GAMEPAD_BUTTON_LEFT_FACE_UP:     return {10, -1, -1, -1}; // D-pad, common GLFW raw order
        case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:  return {11, -1, -1, -1};
        case GAMEPAD_BUTTON_LEFT_FACE_DOWN:   return {12, -1, -1, -1};
        case GAMEPAD_BUTTON_LEFT_FACE_LEFT:   return {13, -1, -1, -1};
        case GAMEPAD_BUTTON_LEFT_THUMB:       return {8, -1, -1, -1};
        case GAMEPAD_BUTTON_RIGHT_THUMB:      return {9, -1, -1, -1};
        default: return {-1, -1, -1, -1};
    }
}

bool raw_button_down(const InputState& input, int button, bool previous)
{
    const auto indices = raw_button_indices_for_gamepad_button(button);
    const auto& buttons = previous ? input.prev_raw_buttons : input.raw_buttons;
    for (int raw_index : indices) {
        if (raw_index >= 0 && raw_index < input.button_count && buttons[raw_index]) {
            return true;
        }
    }
    return false;
}

unsigned char raw_hat_mask_for_gamepad_button(int button)
{
    switch (button) {
        case GAMEPAD_BUTTON_LEFT_FACE_UP:    return GLFW_HAT_UP;
        case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return GLFW_HAT_RIGHT;
        case GAMEPAD_BUTTON_LEFT_FACE_DOWN:  return GLFW_HAT_DOWN;
        case GAMEPAD_BUTTON_LEFT_FACE_LEFT:  return GLFW_HAT_LEFT;
        default: return 0;
    }
}

bool raw_hat_dpad_down(const InputState& input, int button, bool previous)
{
    unsigned char mask = raw_hat_mask_for_gamepad_button(button);
    if (mask == 0 || input.hat_count <= 0) return false;

    const auto& hats = previous ? input.prev_raw_hats : input.raw_hats;
    for (int h = 0; h < input.hat_count; h++) {
        if ((hats[h] & mask) == mask) return true;
    }
    return false;
}

bool raw_axis_dpad_down(const InputState& input, int button, bool previous)
{
    if (input.axis_count <= 7) return false;

    const auto& axes = previous ? input.prev_raw_axes : input.raw_axes;
    switch (button) {
        case GAMEPAD_BUTTON_LEFT_FACE_LEFT:  return axes[6] < -0.5f;
        case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return axes[6] >  0.5f;
        case GAMEPAD_BUTTON_LEFT_FACE_UP:    return axes[7] < -0.5f;
        case GAMEPAD_BUTTON_LEFT_FACE_DOWN:  return axes[7] >  0.5f;
        default: return false;
    }
}

bool key_active(int key)
{
    return IsKeyDown(key) || IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

bool keyboard_control_requested()
{
    return key_active(KEY_W) || key_active(KEY_S)
        || key_active(KEY_A) || key_active(KEY_D)
        || key_active(KEY_Q) || key_active(KEY_E)
        || key_active(KEY_R) || key_active(KEY_F)
        || key_active(KEY_UP) || key_active(KEY_DOWN)
        || key_active(KEY_LEFT) || key_active(KEY_RIGHT)
        || key_active(KEY_ONE) || key_active(KEY_TWO) || key_active(KEY_THREE)
        || key_active(KEY_FOUR) || key_active(KEY_FIVE) || key_active(KEY_SIX)
        || key_active(KEY_Z) || key_active(KEY_X) || key_active(KEY_C)
        || key_active(KEY_TAB)
        || key_active(KEY_EQUAL) || key_active(KEY_MINUS)
        || key_active(KEY_KP_ADD) || key_active(KEY_KP_SUBTRACT);
}

bool controller_control_requested(const InputState& input)
{
    if (!input.device_available) return false;

    constexpr float AXIS_EPSILON = 0.001f;
    if (std::fabs(input.left_x) > AXIS_EPSILON
        || std::fabs(input.left_y) > AXIS_EPSILON
        || std::fabs(input.right_x) > AXIS_EPSILON
        || std::fabs(input.right_y) > AXIS_EPSILON
        || input.left_trigger > AXIS_EPSILON
        || input.right_trigger > AXIS_EPSILON) {
        return true;
    }

    return gamepad_down(input, GAMEPAD_BUTTON_LEFT_FACE_UP)
        || gamepad_down(input, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)
        || gamepad_down(input, GAMEPAD_BUTTON_LEFT_FACE_DOWN)
        || gamepad_down(input, GAMEPAD_BUTTON_LEFT_FACE_LEFT)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_FACE_UP)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)
        || gamepad_down(input, GAMEPAD_BUTTON_LEFT_TRIGGER_1)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
        || gamepad_down(input, GAMEPAD_BUTTON_LEFT_THUMB)
        || gamepad_down(input, GAMEPAD_BUTTON_RIGHT_THUMB);
}

void populate_raw_joystick_state(InputState& input)
{
    int axis_count = 0;
    const float* axes = glfwGetJoystickAxes(input.device, &axis_count);
    int button_count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(input.device, &button_count);
    int hat_count = 0;
    const unsigned char* hats = glfwGetJoystickHats(input.device, &hat_count);

    static std::array<std::array<float, config::MaxRawAxesToTrack>, MAX_JOYSTICKS_TO_SCAN> previous_axes = {};
    static std::array<std::array<bool, config::MaxRawButtonsToTrack>, MAX_JOYSTICKS_TO_SCAN> previous_buttons = {};
    static std::array<std::array<unsigned char, config::MaxRawHatsToTrack>, MAX_JOYSTICKS_TO_SCAN> previous_hats = {};

    input.prev_raw_axes = previous_axes[input.device];
    input.prev_raw_buttons = previous_buttons[input.device];
    input.prev_raw_hats = previous_hats[input.device];

    input.axis_count = std::min(axis_count, config::MaxRawAxesToTrack);
    input.button_count = std::min(button_count, config::MaxRawButtonsToTrack);
    input.hat_count = std::min(hat_count, config::MaxRawHatsToTrack);

    if (axes != nullptr) {
        for (int a = 0; a < input.axis_count; a++) {
            input.raw_axes[a] = axes[a];
        }
    }
    if (buttons != nullptr) {
        for (int b = 0; b < input.button_count; b++) {
            input.raw_buttons[b] = buttons[b] == GLFW_PRESS;
        }
    }
    if (hats != nullptr) {
        for (int h = 0; h < input.hat_count; h++) {
            input.raw_hats[h] = hats[h];
        }
    }

    previous_axes[input.device] = input.raw_axes;
    previous_buttons[input.device] = input.raw_buttons;
    previous_hats[input.device] = input.raw_hats;
}

} // namespace

void apply_wifi_controller_snapshot(InputState& input,
                                    const WifiControllerSnapshot& wifi)
{
    input.wifi_available = wifi.server_enabled;
    input.wifi_connected = wifi.client_connected;
    input.wifi_active = wifi.active;
    input.wifi_relay_status = wifi.relay_status;
    input.wifi_port = wifi.port;
    input.wifi_update_count = wifi.update_count;
    input.wifi_seconds_since_update = wifi.seconds_since_update;
    input.wifi_height = static_cast<float>(wifi.height);
    input.wifi_target_height = static_cast<float>(wifi.target_height);
    input.wifi_height_control_active = wifi.height_control_active;
    input.wifi_height_control_gentle = wifi.height_control_gentle;
    input.wifi_body_radius = static_cast<float>(wifi.target_body_radius);
    input.wifi_body_radius_control_active = wifi.body_radius_control_active;
    input.wifi_step_height = static_cast<float>(wifi.target_step_height);
    input.wifi_step_height_control_active = wifi.step_height_control_active;
    input.wifi_speed = static_cast<float>(wifi.target_speed);
    input.wifi_speed_control_active = wifi.speed_control_active;
    input.wifi_position = wifi.position;
    input.wifi_target_position = wifi.target_position;
    input.wifi_position_control_active = wifi.position_control_active;
    input.wifi_gait = wifi.target_gait;
    input.wifi_gait_control_active = wifi.gait_control_active;
    input.wifi_voltage = static_cast<float>(wifi.voltage);
    input.wifi_current = static_cast<float>(wifi.current);
    input.wifi_primary_x = static_cast<float>(wifi.primary.x);
    input.wifi_primary_y = static_cast<float>(wifi.primary.y);
    input.wifi_secondary_x = static_cast<float>(wifi.secondary.x);
    input.wifi_secondary_y = static_cast<float>(wifi.secondary.y);
    input.wifi_primary_x_action = wifi.primary_x_action;
    input.wifi_primary_y_action = wifi.primary_y_action;
    input.wifi_secondary_x_action = wifi.secondary_x_action;
    input.wifi_secondary_y_action = wifi.secondary_y_action;
}

InputState read_input_state()
{
    InputState input;
    for (int i = 0; i < config::MaxGamepadsToScan; i++) {
        if (IsGamepadAvailable(i) && glfwJoystickIsGamepad(i)) {
            input.device_available = true;
            input.mapped_gamepad = true;
            input.device = i;
            break;
        }
    }

    if (input.mapped_gamepad) {
        populate_raw_joystick_state(input);
        input.left_x = apply_deadzone(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_LEFT_X),
            config::GamepadStickDeadzone);
        input.left_y = apply_deadzone(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_LEFT_Y),
            config::GamepadStickDeadzone);
        input.right_x = apply_deadzone(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_RIGHT_X),
            config::GamepadStickDeadzone);
        input.right_y = apply_deadzone(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_RIGHT_Y),
            config::GamepadStickDeadzone);
        input.left_trigger = trigger_pressure(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_LEFT_TRIGGER));
        input.right_trigger = trigger_pressure(
            GetGamepadAxisMovement(input.device, GAMEPAD_AXIS_RIGHT_TRIGGER));
        return input;
    }

    for (int i = 0; i < MAX_JOYSTICKS_TO_SCAN; i++) {
        if (!glfwJoystickPresent(i)) continue;

        int axis_count = 0;
        const float* axes = glfwGetJoystickAxes(i, &axis_count);
        int button_count = 0;
        const unsigned char* buttons = glfwGetJoystickButtons(i, &button_count);
        int hat_count = 0;
        const unsigned char* hats = glfwGetJoystickHats(i, &hat_count);
        if ((axes != nullptr && axis_count >= 2)
            || (buttons != nullptr && button_count > 0)
            || (hats != nullptr && hat_count > 0)) {
            input.device_available = true;
            input.raw_joystick = true;
            input.device = i;
            populate_raw_joystick_state(input);

            if (axes != nullptr) {
                input.left_x = apply_deadzone(axis_count > 0 ? axes[0] : 0.0f, config::GamepadStickDeadzone);
                input.left_y = apply_deadzone(axis_count > 1 ? axes[1] : 0.0f, config::GamepadStickDeadzone);
                input.right_x = apply_deadzone(axis_count > 2 ? axes[2] : 0.0f, config::GamepadStickDeadzone);
                input.right_y = apply_deadzone(axis_count > 3 ? axes[3] : 0.0f, config::GamepadStickDeadzone);
                input.left_trigger = axis_count > 4 ? raw_trigger_pressure(axes[4]) : 0.0f;
                input.right_trigger = axis_count > 5 ? raw_trigger_pressure(axes[5]) : 0.0f;
            }
            break;
        }
    }

    return input;
}

ControlInputState update_control_input_source(const InputState& input,
                                              ControlInputState previous)
{
    ControlInputState next = previous;
    next.keyboard_requested = keyboard_control_requested();
    next.controller_requested = controller_control_requested(input);
    next.wifi_requested = input.wifi_connected || input.wifi_active;

    const bool keyboard_started = next.keyboard_requested && !previous.keyboard_requested;
    const bool controller_started = next.controller_requested && !previous.controller_requested;
    const bool wifi_started = next.wifi_requested && !previous.wifi_requested;

    if (keyboard_started) {
        next.active_source = ControlInputSource::KEYBOARD;
    } else if (controller_started) {
        next.active_source = ControlInputSource::CONTROLLER;
    } else if (wifi_started) {
        next.active_source = ControlInputSource::WIFI;
    } else {
        auto active_source_still_requested = [&]() {
            switch (previous.active_source) {
                case ControlInputSource::KEYBOARD: return next.keyboard_requested;
                case ControlInputSource::CONTROLLER: return next.controller_requested;
                case ControlInputSource::WIFI: return next.wifi_requested;
            }
            return false;
        };

        if (active_source_still_requested()) {
            next.active_source = previous.active_source;
        } else if (next.controller_requested) {
            next.active_source = ControlInputSource::CONTROLLER;
        } else if (next.wifi_requested) {
            next.active_source = ControlInputSource::WIFI;
        } else if (next.keyboard_requested) {
            next.active_source = ControlInputSource::KEYBOARD;
        } else if (previous.active_source == ControlInputSource::CONTROLLER && !input.device_available) {
            next.active_source = ControlInputSource::KEYBOARD;
        } else if (previous.active_source == ControlInputSource::WIFI && !input.wifi_connected) {
            next.active_source = ControlInputSource::KEYBOARD;
        } else {
            next.active_source = previous.active_source;
        }
    }

    return next;
}

bool gamepad_pressed(const InputState& input, int button)
{
    if (input.mapped_gamepad && IsGamepadButtonPressed(input.device, button)) return true;

    bool raw_button_pressed = input.device_available
        && raw_button_down(input, button, false)
        && !raw_button_down(input, button, true);

    bool raw_hat_pressed = raw_hat_dpad_down(input, button, false)
        && !raw_hat_dpad_down(input, button, true);
    bool raw_axis_pressed = raw_axis_dpad_down(input, button, false)
        && !raw_axis_dpad_down(input, button, true);

    return raw_button_pressed || raw_hat_pressed || raw_axis_pressed;
}

bool gamepad_down(const InputState& input, int button)
{
    if (input.mapped_gamepad && IsGamepadButtonDown(input.device, button)) return true;

    bool raw_button_is_down = input.device_available
        && raw_button_down(input, button, false);

    return raw_button_is_down
        || raw_hat_dpad_down(input, button, false)
        || raw_axis_dpad_down(input, button, false);
}

const char* input_device_name(const InputState& input)
{
    if (!input.device_available) return nullptr;
    if (input.mapped_gamepad) return GetGamepadName(input.device);
    return glfwGetJoystickName(input.device);
}
