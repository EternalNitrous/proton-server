#pragma once

#include "wifi_controller.h"

struct InputState {
    bool wifi_available = false;
    bool wifi_connected = false;
    bool wifi_active = false;
    bool wifi_relay_status = false;
    int wifi_port = 0;
    int wifi_update_count = 0;
    double wifi_seconds_since_update = 0.0;
    float wifi_height = 0.0f;
    float wifi_target_height = 0.0f;
    bool wifi_height_control_active = false;
    bool wifi_height_control_gentle = false;
    float wifi_body_radius = 0.0f;
    bool wifi_body_radius_control_active = false;
    float wifi_step_height = 0.0f;
    bool wifi_step_height_control_active = false;
    float wifi_speed = 0.0f;
    bool wifi_speed_control_active = false;
    int wifi_position = 0;
    int wifi_target_position = 0;
    bool wifi_position_control_active = false;
    int wifi_gait = 1;
    bool wifi_gait_control_active = false;
    float wifi_voltage = 0.0f;
    float wifi_current = 0.0f;
    float wifi_primary_x = 0.0f;
    float wifi_primary_y = 0.0f;
    float wifi_secondary_x = 0.0f;
    float wifi_secondary_y = 0.0f;
    WifiAxisAction wifi_primary_x_action = WifiAxisAction::TURN;
    WifiAxisAction wifi_primary_y_action = WifiAxisAction::MARCH;
    WifiAxisAction wifi_secondary_x_action = WifiAxisAction::STRAFE;
    WifiAxisAction wifi_secondary_y_action = WifiAxisAction::NONE;
};

enum class ControlInputSource {
    NONE,
    WIFI
};

struct ControlInputState {
    ControlInputSource active_source = ControlInputSource::NONE;
    bool wifi_requested = false;
};

InputState read_input_state();
void apply_wifi_controller_snapshot(InputState& input,
                                    const WifiControllerSnapshot& wifi);
ControlInputState update_control_input_source(const InputState& input,
                                              ControlInputState previous);
