#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
#endif

#include "kinematics.h"
#include "mdns_responder.h"
#include "servo.h"

#ifdef _WIN32
using WifiSocket = std::uintptr_t;
inline constexpr WifiSocket InvalidWifiSocket = ~WifiSocket{0};
#else
using WifiSocket = int;
inline constexpr WifiSocket InvalidWifiSocket = -1;
#endif

struct WifiJoystick {
    double x = 0.0;
    double y = 0.0;
};

enum class WifiAxisAction {
    NONE = 0,
    MARCH,
    STRAFE,
    TURN,
    HEIGHT,
    FRONT_BACK,
    SIDE_SIDE,
    CIRCLE
};

struct WifiControllerSnapshot {
    bool server_enabled = false;
    bool client_connected = false;
    bool active = false;
    int port = 0;
    int update_count = 0;
    double seconds_since_update = 0.0;
    bool relay_status = false;
    bool target_relay_status = false;
    bool relay_control_active = false;
    bool relay_switch_ready = false;
    bool relay_restore_default_height = true;
    bool relay_asleep_from_inactivity = false;
    double height = 0.0;
    double target_height = 0.0;
    double user_target_height = 0.0;
    bool height_control_active = false;
    bool height_control_gentle = false;
    double body_radius = 0.0;
    double target_body_radius = 0.0;
    bool body_radius_control_active = false;
    double step_height = 0.0;
    double target_step_height = 0.0;
    bool step_height_control_active = false;
    double speed = 0.0;
    double target_speed = 0.0;
    bool speed_control_active = false;
    int position = 0;
    int target_position = 0;
    bool position_control_active = false;
    int gait = 1;
    bool gait_control_active = false;
    int target_gait = 1;
    double voltage = 0.0;
    double current = 0.0;
    WifiJoystick primary;
    WifiJoystick secondary;
    WifiAxisAction primary_x_action = WifiAxisAction::TURN;
    WifiAxisAction primary_y_action = WifiAxisAction::MARCH;
    WifiAxisAction secondary_x_action = WifiAxisAction::STRAFE;
    WifiAxisAction secondary_y_action = WifiAxisAction::NONE;
};

struct VisualizerLegSnapshot {
    LegJoints joints;
    PWMValues pwm;
    Vec3 points[5];
    Vec3 target;
    bool swing = false;
};

struct VisualizerSnapshot {
    bool available = false;
    std::uint64_t sequence = 0;
    double time = 0.0;
    BasePose pose;
    VisualizerLegSnapshot legs[6];
};

class WifiControllerServer {
public:
    ~WifiControllerServer();

    bool start(int port);
    void stop();
    void update_robot_status(double height, int position, int gait);
    void update_motion_status(double body_radius, double step_height, double speed);
    void update_shutdown_complete(bool complete);
    void request_relay_status(bool enabled, bool restore_default_height = true);
    void update_relay_status(bool enabled, bool complete_request = true);
    void update_voltage(double voltage);
    void update_current(double current);
    void update_visualizer_frame(const BasePose& pose,
                                 const LegPoints fk_points[6],
                                 const Vec3 feet_world[6],
                                 const bool is_swing[6],
                                 const RobotState& render_state,
                                 const std::array<PWMValues, 6>& render_pwm);
    WifiControllerSnapshot snapshot() const;
    std::string visualizer_json() const;
    const std::string& status() const { return status_; }

private:
    void serve_loop();
    void handle_client(WifiSocket client);
    void handle_websocket(WifiSocket client, const std::string& request);
    void handle_visualizer_websocket(WifiSocket client, const std::string& request);
    std::string handle_system_power_request(const std::string& request, const std::string& action);
    bool update_coordinates(const std::string& body);
    bool remove_visualizer_client(WifiSocket client);
    std::string register_power_key();
    void remove_power_key(const std::string& key);
    bool power_key_is_active(const std::string& key) const;
    void start_mdns(int port);
    void stop_mdns();

    mutable std::mutex state_mutex_;
    WifiControllerSnapshot state_;
    mutable std::mutex visualizer_mutex_;
    VisualizerSnapshot visualizer_;
    std::vector<WifiSocket> visualizer_clients_;
    mutable std::mutex power_key_mutex_;
    std::vector<std::string> active_power_keys_;
    std::string status_ = "disabled";
    std::atomic<bool> running_{false};
    std::atomic<int> websocket_clients_{0};
    std::thread server_thread_;
    WifiSocket server_fd_ = InvalidWifiSocket;
    bool socket_runtime_started_ = false;
    MdnsResponder mdns_responder_;
    std::chrono::steady_clock::time_point last_update_time_ =
        std::chrono::steady_clock::time_point::min();
};
