#pragma once
// ============================================================
//  robot_params.h — hexapod physical parameters
//  Direct port of config/get_robot_params.m
// ============================================================
#include "config.h"
#include "types.h"
#include <cmath>

// ---- Link dimensions -------------------------------------------
struct RobotDims {
    double coxa_len     = config::CoxaLength;
    double femur_len    = config::FemurLength;
    double tibia_len    = config::TibiaLength;
};

// ---- Body geometry (for visualiser) ----------------------------
struct BodyShape {
    double length  = config::BodyLength;
    double width   = config::BodyWidth;
    double chamfer = config::BodyChamfer;
};

// ---- Full robot parameter set ----------------------------------
struct RobotParams {
    RobotDims dims;
    BodyShape body;
    double mount_angles[6];               // CCW from forward (X+) axis, radians
    double mount_radii[6];                // distance from body centre to coxa pivot, m
    Vec3   default_foot_positions[6];     // default foot positions in world frame
    double l_horiz = 0.0;                  // neutral horizontal reach from coxa pivot
};

// ---- Factory function (mirrors get_robot_params.m) -------------
//
//  LEG LAYOUT (angles from +X / forward axis):
//   Leg  Name  Angle
//    0   R1    -36.297 deg  (front-right)
//    1   R2    -90 deg  (mid-right)
//    2   R3   -143.703 deg  (rear-right)
//    3   L1    +36.297 deg  (front-left)
//    4   L2    +90 deg  (mid-left)
//    5   L3   +143.703 deg  (rear-left)
//
inline RobotParams get_robot_params()
{
    RobotParams p;

    // ---- Tuning ---------------------------------------------------
    constexpr double TARGET_KNEE_DEG = config::DefaultKneeAngleDeg;
    constexpr double TARGET_BODY_Z   = config::DefaultBodyHeight;
    const     double D2R             = M_PI / 180.0;

    // ---- Mount angles (radians) -----------------------------------
    for (int i = 0; i < 6; i++) {
        p.mount_angles[i] = config::MountAnglesDeg[i] * D2R;
    }

    // ---- Mount radii (metres) ------------------------------------
    for (int i = 0; i < 6; i++) {
        p.mount_radii[i] = config::MountRadii[i];
    }

    // ---- Default foot positions ----------------------------------
    // Derived from TARGET_KNEE_DEG so the knee rests at the target angle.
    auto& d = p.dims;
    double tibia_target = (TARGET_KNEE_DEG - 180.0) * D2R;
    double lcz = std::sqrt(d.femur_len*d.femur_len + d.tibia_len*d.tibia_len
                           + 2.0*d.femur_len*d.tibia_len*std::cos(tibia_target));
    double lc_sq = lcz*lcz - TARGET_BODY_Z*TARGET_BODY_Z;
    if (lc_sq < 0.0) lc_sq = 0.0;
    p.l_horiz = std::sqrt(lc_sq) + d.coxa_len;

    for (int i = 0; i < 6; i++) {
        double ma = p.mount_angles[i];
        double coxa_neutral_angle = ma + config::CoxaAngleOffsetsDeg[i] * D2R;
        double mr = p.mount_radii[i];
        double cx_b = mr * std::cos(ma);
        double cy_b = mr * std::sin(ma);
        p.default_foot_positions[i].x = cx_b + p.l_horiz * std::cos(coxa_neutral_angle);
        p.default_foot_positions[i].y = cy_b + p.l_horiz * std::sin(coxa_neutral_angle);
        p.default_foot_positions[i].z = 0.0;
    }

    return p;
}
