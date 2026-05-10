#pragma once
// ============================================================
//  types.h — core data structures for the hexapod simulation
//  Mirrors the MATLAB struct layout exactly.
// ============================================================
#include "config.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class GaitType { TRIPOD = 0, RIPPLE = 1, AMBLE = 2, RIPPLE_EXT = 3 };
enum class DanceMode { NONE = 0, FRONT_BACK = 1, SIDE_SIDE = 2, CIRCLE = 3 };

// ---- 3-D point / vector ----------------------------------------
struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(const Vec3& b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(double s)      const { return {x*s,   y*s,   z*s};   }
    double norm() const { return std::sqrt(x*x + y*y + z*z); }
};

// ---- Body pose in world frame ----------------------------------
// Matches MATLAB's base_pose / final_body_pose structs
struct BasePose {
    double x = 0, y = 0, z = 0.12;
    double roll = 0, pitch = 0, yaw = 0;
};

// ---- Per-leg joint angles (radians) ----------------------------
// Mirrors MATLAB's robot_state.legs(i)
struct LegJoints {
    double hip   = 0;      // always 0 (no hip joint on this platform)
    double coxa  = 0;
    double femur = 0;
    double tibia = 0;
};

// ---- Full robot state ------------------------------------------
struct RobotState {
    LegJoints legs[6];
};

// ---- Gait state (persistent across frames) ---------------------
// Mirrors MATLAB's gait_state struct
struct GaitState {
    bool   initialized = false;
    double master_phase = 0;
    GaitType current_mode = GaitType::TRIPOD;

    // Odometry (world frame)
    double pose_x   = 0;
    double pose_y   = 0;
    double pose_yaw = 0;

    // Smoothed command velocities
    double smooth_vx = 0, smooth_vy = 0, smooth_wz = 0;

    // Body sway
    double sway_roll  = 0, sway_pitch = 0;
    double prev_vx_w  = 0, prev_vy_w  = 0;

    // Per-leg state
    bool  is_swing[6]    = {};
    Vec3  swing_start[6] = {};
    Vec3  feet_world[6]  = {};
    double drag_distance[6] = {};
};

// ---- Command structure -----------------------------------------
// Mirrors MATLAB's cmd struct (keyboard.m / main_testing.m)
struct Command {
    struct GaitCmd {
        double cycle_time   = config::Motion.cycle_time;
        double step_height  = config::Motion.step_height;
        double stride_scale = 1.0;
    } gait;

    struct PoseCmd {
        double x = 0, y = 0, z = config::Motion.start_height;
        double roll = 0, pitch = 0, yaw = 0; // body orientation offset
    } pose;

    struct TwistCmd {
        double linear_x  = 0;
        double linear_y  = 0;
        double angular_z = 0;
    } twist;

    double body_radius = config::Motion.body_radius;
};
