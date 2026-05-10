#pragma once

#include "input.h"
#include "kinematics.h"
#include "options.h"
#include "robot_params.h"
#include "servo.h"
#include "types.h"

#include "raylib_compat.h"

#include <array>

struct HudContext {
    int screen_width;
    int screen_height;
    const AppOptions& options;
    const InputState& input;
    const Servo2040Client& servo2040;
    bool keyboard_enabled;
    bool wifi_enabled;
    const Command& cmd;
    const BasePose& final_pose;
    const RobotState& render_state;
    const GaitState& gait_state;
    const std::array<PWMValues, 6>& render_pwm;
    GaitType gait_type;
    DanceMode active_dance;
    double current_walk_speed;
    double current_strafe_speed;
    double current_spin_rate;
    double max_err;
    double max_drag;
    int selected_pwm_leg;
    int selected_pwm_joint;
    bool ctrl;
    bool shift;
    bool kW;
    bool kS;
    bool kA;
    bool kD;
    bool kQ;
    bool kE;
    bool kR;
    bool kF;
    bool voltage_valid;
    double servo_voltage;
    bool current_valid;
    double servo_current;
    bool voltage_warning;
    bool voltage_critical;
};

Vector3 to_rl(double x, double y, double z);
Vector3 to_rl(const Vec3& v);

void record_footprints(const Vec3 feet[6], const bool is_swing[6], float dt);
void draw_scene(const RobotParams& params,
                const BasePose& final_pose,
                const LegPoints fk_pts[6],
                const Vec3 feet_world[6],
                const bool is_swing[6]);
void draw_hud(const HudContext& hud);
