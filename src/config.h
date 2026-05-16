#pragma once
// ============================================================
//  config.h — user-tunable settings for your hexapod
//
//  Start here when adapting the simulator to your robot:
//    - link lengths, body size, and leg mount layout
//    - neutral pose and motion limits
//    - servo angle/PWM calibration
//    - Servo2040 pin order and hardware direction flips
// ============================================================
#include <array>

namespace config {

// ---- Window / camera ------------------------------------------
inline constexpr int ScreenWidth = 1280;
inline constexpr int ScreenHeight = 800;
inline constexpr float CameraAzimuthDeg = 45.0f;
inline constexpr float CameraElevationDeg = 30.0f;
inline constexpr float CameraMinElevationDeg = 5.0f;
inline constexpr float CameraMaxElevationDeg = 85.0f;
inline constexpr float CameraDistance = 0.85f;
inline constexpr float CameraFovy = 55.0f;
inline constexpr float CameraMinDistance = 0.30f;
inline constexpr float CameraMaxDistance = 3.00f;

// ---- Physical robot geometry (metres, degrees) ----------------
inline constexpr double CoxaLength = 0.043;
inline constexpr double FemurLength = 0.080;
inline constexpr double TibiaLength = 0.134;

inline constexpr double BodyLength = 0.140;
inline constexpr double BodyWidth = 0.100;
inline constexpr double BodyChamfer = 0.020;

// Simulation leg order: R1, R2, R3, L1, L2, L3.
inline constexpr std::array<double, 6> MountAnglesDeg = {
    -36.297, -90.000, -143.703, 36.297, 90.000, 143.703
};
inline constexpr std::array<double, 6> MountRadii = {
    0.094123, 0.071500, 0.094123, 0.094123, 0.071500, 0.094123
};

// Per-leg coxa output offsets, in degrees.
// Leg order: R1, R2, R3, L1, L2, L3. Positive rotates CCW from each mount angle.
inline constexpr std::array<double, 6> CoxaAngleOffsetsDeg = {
    12.0, 0.0, -12.0, -12.0, 0.0, 12.0
};

// Neutral stance geometry used to derive default foot XY positions.
// Runtime standing height is Motion.start_height, not NeutralStanceBodyHeight.
inline constexpr double NeutralStanceKneeAngleDeg = 115.0;
inline constexpr double NeutralStanceBodyHeight = 0.12;
inline constexpr double DefaultKneeAngleDeg = NeutralStanceKneeAngleDeg;
inline constexpr double DefaultBodyHeight = NeutralStanceBodyHeight;
inline constexpr double SitBodyHeight = 0.02;

// ---- IK and workspace limits (degrees) ------------------------
inline constexpr double SafeCoxaLimitDeg = 72.0;
inline constexpr double SafeTibiaFoldedDeg = -140.0;
inline constexpr double SafeTibiaExtendedDeg = -15.0;
inline constexpr double FemurMinDeg = -85.0;
inline constexpr double FemurMaxDeg = 85.0;

// ---- Locomotion defaults --------------------------------------
struct MotionConfig {
    double cycle_time;
    double step_height;
    double body_radius;
    double start_height;
    double height_min;
    double height_max;
    double height_rate;
    double walk_speed;
    double strafe_speed;
    double spin_rate;
    double creep_scale;
    double linear_speed_min;
    double linear_speed_max;
    double spin_rate_min;
    double spin_rate_max;
    double linear_speed_adjust_rate;
    double spin_rate_adjust_rate;
    double smooth_rate;
    double dance_sway_angle;
    double dance_circle_angle;
    double dance_shift_amount;
    double dance_rate_hz;
};

inline constexpr MotionConfig Motion = {
    2.0,        // cycle_time
    0.08,       // step_height
    0.15,       // body_radius
    0.056,      // start_height
    0.05,       // height_min
    0.18,       // height_max
    0.06,       // height_rate
    0.15,       // walk_speed
    0.15,       // strafe_speed
    1.0,        // spin_rate
    2.0 / 3.0,  // creep_scale
    0.06,       // linear_speed_min
    0.225,      // linear_speed_max
    0.5,        // spin_rate_min
    1.25,       // spin_rate_max
    0.08,       // linear_speed_adjust_rate
    0.40,       // spin_rate_adjust_rate
    8.0,        // smooth_rate
    0.18,       // dance_sway_angle
    0.15,       // dance_circle_angle
    0.012,      // dance_shift_amount
    1.15        // dance_rate_hz
};

// ---- Per-gait tuning ------------------------------------------
struct GaitEngineConfig {
    double cmd_smooth_hz;
    double step_height_full_speed;
    double step_height_min_factor;
    double max_safe_stride;
    double spin_safe_stride;
    double reach_multiplier;
    double pitch_accel_gain;
    double roll_centripetal_gain;
    double roll_lateral_gain;
    double max_sway_angle;
    double sway_response_hz;
    double phase_roll_amp;
    double coxa_limit_deg;
    double femur_reach_margin;
    double max_linear_speed;
    double max_angular_speed;
};

inline constexpr GaitEngineConfig GaitEngine = {
    2.0,    // cmd_smooth_hz
    0.17,   // step_height_full_speed
    0.25,   // step_height_min_factor
    0.075,  // max_safe_stride
    0.115,  // spin_safe_stride
    1.11,   // reach_multiplier
    0.25,   // pitch_accel_gain
    0.20,   // roll_centripetal_gain
    0.15,   // roll_lateral_gain
    0.08,   // max_sway_angle
    1.5,    // sway_response_hz
    0.010,  // phase_roll_amp
    68.0,   // coxa_limit_deg
    0.93,   // femur_reach_margin
    0.25,   // max_linear_speed
    2.0     // max_angular_speed
};

// ---- Input tuning ---------------------------------------------
inline constexpr int MaxGamepadsToScan = 4;
inline constexpr int MaxRawAxesToTrack = 16;
inline constexpr int MaxRawButtonsToTrack = 32;
inline constexpr int MaxRawHatsToTrack = 4;
inline constexpr float GamepadStickDeadzone = 0.14f;
inline constexpr float GamepadTriggerDeadzone = 0.08f;

// ---- Servo calibration ----------------------------------------
inline constexpr double ServoCoxaCenterDeg = 135.0;
inline constexpr double ServoFemurCenterDeg = 100.0;
inline constexpr double ServoTibiaCenterDeg = 225.0;
inline constexpr int PwmMin = 500;
inline constexpr int PwmMax = 2500;
inline constexpr int PwmNeutral = 1500;
inline constexpr double ServoAngleRangeDeg = 270.0;

// Servo2040 hardware pin order. Each pin maps to one simulated leg and joint.
// Joint indices: 0=coxa, 1=femur, 2=tibia.
inline constexpr std::array<int, 18> Servo2040PinLeg = {
    2, 2, 2, 5, 5, 5, 1, 1, 1, 4, 4, 4, 0, 0, 0, 3, 3, 3
};
inline constexpr std::array<int, 18> Servo2040PinJoint = {
    0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2
};
inline constexpr std::array<bool, 18> Servo2040FlipForHardware = {
    true, false, true,
    true, true,  false,
    true, false, true,
    true, true,  false,
    true, false, true,
    true, true,  false
};
inline constexpr int Servo2040FlipPwmSum = 3000;
inline constexpr double Servo2040VoltagePollInterval = 0.50;
inline constexpr double Servo2040VoltageStartupDelay = 3.0;
inline constexpr int Servo2040VoltageCriticalSamples = 2;
inline constexpr double Servo2040VoltageWarn = 7.0;
inline constexpr double Servo2040VoltageCritical = 6.0;
inline constexpr double Servo2040VoltageScale = 310.3;
inline constexpr int Servo2040CurrentOffset = 512;
inline constexpr double Servo2040CurrentScale = 0.0814;
inline constexpr double VoltageWarningPulseSeconds = 5.0;

// ---- Rendering -------------------------------------------------
inline constexpr float FootprintLifetime = 12.0f;
inline constexpr int MaxFootprints = 2000;

} // namespace config
