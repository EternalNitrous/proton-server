#include "input.h"

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
    return {};
}

ControlInputState update_control_input_source(const InputState& input,
                                              ControlInputState previous)
{
    ControlInputState next = previous;
    next.wifi_requested = input.wifi_active;

    if (next.wifi_requested) {
        next.active_source = ControlInputSource::WIFI;
    } else if (previous.active_source == ControlInputSource::WIFI && !input.wifi_connected) {
        next.active_source = ControlInputSource::NONE;
    }

    return next;
}
