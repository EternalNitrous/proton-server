#include "visual.h"

#include "config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

Vector3 to_rl(double x, double y, double z)
{
    return { (float)x, (float)z, (float)(-y) };
}

Vector3 to_rl(const Vec3& v)
{
    return to_rl(v.x, v.y, v.z);
}

namespace {

constexpr const char* GAIT_NAMES[4] = {"Tripod", "Ripple", "Amble", "RippleExt"};
constexpr const char* LEG_NAMES[6] = {"R1", "R2", "R3", "L1", "L2", "L3"};
constexpr const char* JOINT_NAMES[3] = {"coxa", "femur", "tibia"};

const char* dance_mode_name(DanceMode mode)
{
    switch (mode) {
        case DanceMode::FRONT_BACK: return "Front/back";
        case DanceMode::SIDE_SIDE:  return "Side/side";
        case DanceMode::CIRCLE:     return "Circle";
        case DanceMode::NONE:
        default:                    return "Off";
    }
}

struct FootPrint {
    float x, y;
    int leg;
    float age;
};

struct HudLine {
    std::string s;
    Color c;
    int x;
    int y;
    int size;
};

std::deque<FootPrint> g_footprints;
bool g_prev_swing[6] = {};

const Color LEG_COLORS[6] = {
    {220,  80,  80, 255},
    {220, 140,  50, 255},
    {180,  80, 220, 255},
    {50,  160, 220, 255},
    {50,  200, 100, 255},
    {200, 180,  50, 255},
};

void draw_body(const BasePose& bp, const RobotParams& p)
{
    float bx = (float)(p.body.length  / 2.0);
    float by = (float)(p.body.width   / 2.0);
    float ch = (float)p.body.chamfer;

    float bvx[8] = { bx,      bx-ch,  -(bx-ch), -bx,    -bx,   -(bx-ch),  bx-ch,    bx   };
    float bvy[8] = {-(by-ch), -by,    -by,      -(by-ch), (by-ch), by,       by,    (by-ch)};

    float cy = (float)std::cos(bp.yaw);
    float sy = (float)std::sin(bp.yaw);

    Vector3 verts[8];
    for (int i = 0; i < 8; i++) {
        float wx = bvx[i]*cy - bvy[i]*sy + (float)bp.x;
        float wy = bvx[i]*sy + bvy[i]*cy + (float)bp.y;
        verts[i] = to_rl(wx, wy, (float)bp.z);
    }

    Vector3 ctr = to_rl((float)bp.x, (float)bp.y, (float)bp.z);
    Color face_col = {77, 77, 77, 204};
    for (int i = 0; i < 8; i++) {
        int j = (i + 1) % 8;
        DrawTriangle3D(ctr, verts[i], verts[j], face_col);
        DrawTriangle3D(ctr, verts[j], verts[i], face_col);
    }

    for (int i = 0; i < 8; i++) {
        int j = (i + 1) % 8;
        DrawLine3D(verts[i], verts[j], BLACK);
    }
}

void draw_leg(const LegPoints& lp, bool is_swing)
{
    Color seg_col = is_swing ? Color{51, 153, 204, 255}
                             : Color{41, 128, 185, 255};

    float radius_seg  = 0.003f;
    float radius_joint = 0.006f;

    for (int s = 0; s < 4; s++) {
        Vector3 a = to_rl(lp.pts[s]);
        Vector3 b = to_rl(lp.pts[s+1]);
        DrawCylinderEx(a, b, radius_seg, radius_seg, 6, seg_col);
    }

    for (int s = 0; s < 5; s++) {
        DrawSphere(to_rl(lp.pts[s]), radius_joint, BLACK);
    }
}

void draw_foot_target(const Vec3& foot, bool is_swing)
{
    Color col = is_swing ? RED : Color{180, 30, 30, 255};
    DrawSphere(to_rl(foot), 0.009f, col);
}

void draw_grid(float cx, float cy, float extent, float spacing)
{
    float ox = floorf(cx / spacing) * spacing;
    float oy = floorf(cy / spacing) * spacing;

    int n = (int)(extent / spacing) + 2;
    Color maj = {160, 160, 175, 210};
    Color min_ = {205, 205, 215, 80};

    for (int i = -n; i <= n; i++) {
        float tx = ox + (float)i * spacing;
        float ty = oy + (float)i * spacing;
        bool major = (((int)roundf(tx / spacing)) % 5 == 0);
        bool majy  = (((int)roundf(ty / spacing)) % 5 == 0);
        DrawLine3D(to_rl(ox-(float)n*spacing, ty, 0.001f),
                   to_rl(ox+(float)n*spacing, ty, 0.001f), majy ? maj : min_);
        DrawLine3D(to_rl(tx, oy-(float)n*spacing, 0.001f),
                   to_rl(tx, oy+(float)n*spacing, 0.001f), major ? maj : min_);
    }

    DrawLine3D(to_rl(-1.0f, 0.0f, 0.003f), to_rl(1.0f, 0.0f, 0.003f), {220, 50, 50, 240});
    DrawLine3D(to_rl(0.0f, -1.0f, 0.003f), to_rl(0.0f, 1.0f, 0.003f), {50, 200, 50, 240});
    DrawSphere(to_rl(0.0f, 0.0f, 0.003f), 0.012f, {255, 200, 0, 240});
}

void draw_footprints()
{
    for (const auto& fp : g_footprints) {
        float t     = 1.0f - fp.age / config::FootprintLifetime;
        Color base  = LEG_COLORS[fp.leg];
        Color col   = {base.r, base.g, base.b, (unsigned char)(t * 180)};
        float r     = 0.007f + (1.0f - t) * 0.004f;
        DrawSphere(to_rl((double)fp.x, (double)fp.y, 0.002), r, col);
    }
}

void draw_fk_error(const Vec3& fk_foot, const Vec3& target)
{
    if ((fk_foot - target).norm() > 0.0005) {
        DrawLine3D(to_rl(fk_foot), to_rl(target), YELLOW);
    }
}

int hud_width(const std::vector<HudLine>& lines, int pad)
{
    int w = 0;
    for (const HudLine& line : lines) {
        w = std::max(w, MeasureText(line.s.c_str(), line.size));
    }
    return w + pad * 2;
}

void draw_left_panel(const HudContext& hud, int& stable_width)
{
    const bool controller_enabled = !hud.keyboard_enabled && !hud.wifi_enabled;
    int panel_x = 12;
    int py = 12;
    int left_panel_y = 6;
    std::vector<HudLine> left_lines;
    auto text = [&](const char* s, Color c, int sz = 18) {
        left_lines.push_back({s, c, panel_x, py, sz});
        py += sz + 3;
    };

    char buf[256];
    if (hud.options.direct_pwm_control_enabled) {
        snprintf(buf, sizeof(buf), "Mode: Direct PWM");
    } else {
        snprintf(buf, sizeof(buf), "Gait: %s", GAIT_NAMES[(int)hud.gait_type]);
    }
    text(buf, {100, 220, 100, 255}, 20);

    if (hud.wifi_enabled) {
        snprintf(buf, sizeof(buf), "Input   : Active Wi-Fi :%d%s",
                 hud.input.wifi_port,
                 hud.input.wifi_connected ? "" : " (waiting)");
    } else if (controller_enabled && hud.input.device_available) {
        const char* name = input_device_name(hud.input);
        snprintf(buf, sizeof(buf), "Input   : Active %s %d%s%s",
                 hud.input.mapped_gamepad ? "Gamepad" : "Joystick",
                 hud.input.device,
                 name ? " - " : "",
                 name ? name : "");
    } else if (controller_enabled) {
        snprintf(buf, sizeof(buf), "Input   : Active Controller (not connected)");
    } else if (hud.input.device_available) {
        const char* name = input_device_name(hud.input);
        snprintf(buf, sizeof(buf), "Input   : Active Keyboard (%s %d%s%s connected)",
                 hud.input.mapped_gamepad ? "Gamepad" : "Joystick",
                 hud.input.device,
                 name ? " - " : "",
                 name ? name : "");
    } else if (hud.input.wifi_connected) {
        snprintf(buf, sizeof(buf), "Input   : Active Keyboard (Wi-Fi connected :%d)",
                 hud.input.wifi_port);
    } else {
        snprintf(buf, sizeof(buf), "Input   : Active Keyboard");
    }
    text(buf, {100, 200, 255, 255}, 14);
    py += 4;

    double speed = std::sqrt(hud.cmd.twist.linear_x*hud.cmd.twist.linear_x
                            + hud.cmd.twist.linear_y*hud.cmd.twist.linear_y);
    snprintf(buf, sizeof(buf), "Height  : %.3f m", hud.cmd.pose.z); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Set walk: %.3f m/s", hud.current_walk_speed); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Set strf: %.3f m/s", hud.current_strafe_speed); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Set spin: %.3f rad/s", hud.current_spin_rate); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Lin spd : %.3f m/s", speed); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Ang spd : %.3f rad/s", hud.cmd.twist.angular_z); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Dance  : %s", dance_mode_name(hud.active_dance)); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "Yaw     : %.1f deg", hud.final_pose.yaw * (180.0/M_PI)); text(buf, WHITE);
    snprintf(buf, sizeof(buf), "IK err  : %.5f m", hud.max_err);
    text(buf, (hud.max_err > 0.001) ? RED : Color{100, 220, 100, 255});
    snprintf(buf, sizeof(buf), "Leg drag: %.5f m", hud.max_drag);
    text(buf, (hud.max_drag > 0.0) ? YELLOW : Color{100, 220, 100, 255});

    if (hud.options.direct_pwm_control_enabled) {
        PWMValues selected_values = hud.render_pwm[hud.selected_pwm_leg];
        int selected_value = selected_pwm(selected_values, hud.selected_pwm_joint);
        snprintf(buf, sizeof(buf), "Selected: %s %s = %d",
                 LEG_NAMES[hud.selected_pwm_leg], JOINT_NAMES[hud.selected_pwm_joint], selected_value);
        text(buf, Color{100, 200, 255, 255});
        if (hud.options.servo2040_enabled) {
            text("PWM out : direct control", Color{255, 220, 100, 255});
        }
    }
    if (hud.options.servo2040_pwm_sim_enabled) {
        text("PWM sim : Servo2040 loopback", Color{100, 200, 255, 255});
    }
    if (hud.options.servo2040_enabled) {
        snprintf(buf, sizeof(buf), "Servo2040: %s", hud.servo2040.status().c_str());
        Color servo_col = hud.servo2040.is_connected() ? Color{100, 220, 100, 255} : RED;
        text(buf, servo_col);
        if (hud.voltage_valid) {
            snprintf(buf, sizeof(buf), "Voltage : %.2f V", hud.servo_voltage);
            Color voltage_col = Color{100, 220, 100, 255};
            if (hud.voltage_critical) voltage_col = RED;
            else if (hud.voltage_warning) voltage_col = YELLOW;
            text(buf, voltage_col);
        } else {
            text("Voltage : --", Color{180, 180, 180, 255});
        }
        if (hud.current_valid) {
            snprintf(buf, sizeof(buf), "Current : %.2f A", hud.servo_current);
            text(buf, Color{100, 220, 100, 255});
        } else {
            text("Current : --", Color{180, 180, 180, 255});
        }
        if (hud.voltage_critical) {
            text("LOW VOLTAGE SHUTDOWN", RED);
        }
    }
    py += 6;

    text("--- IK angles (deg, tibia ext) ---", {180, 180, 180, 255}, 14);
    for (int i = 0; i < 6; i++) {
        const LegJoints& j = hud.render_state.legs[i];
        snprintf(buf, sizeof(buf), "%s  cx=%+6.1f  fm=%+6.1f  tb=%+6.1f%s",
                 LEG_NAMES[i],
                 j.coxa  * (180.0/M_PI),
                 j.femur * (180.0/M_PI),
                 -j.tibia * (180.0/M_PI),
                 hud.gait_state.is_swing[i] ? " ^" : "  ");
        Color lc = hud.gait_state.is_swing[i] ? Color{100,200,255,255} : Color{200,200,200,255};
        text(buf, lc, 13);
    }
    py += 4;

    text("--- Servo angles (deg) ---", {180, 180, 180, 255}, 14);
    for (int i = 0; i < 6; i++) {
        ServoAngles s = to_servo_angles(i, hud.render_state.legs[i]);
        snprintf(buf, sizeof(buf), "%s  cx=%6.1f  fm=%6.1f  tb=%6.1f",
                 LEG_NAMES[i], s.coxa, s.femur, s.tibia);
        Color lc = hud.gait_state.is_swing[i] ? Color{100,200,255,255} : Color{200,200,200,255};
        text(buf, lc, 13);
    }
    py += 4;

    text("--- PWM values ---", {180, 180, 180, 255}, 14);
    for (int i = 0; i < 6; i++) {
        PWMValues pwm = hud.render_pwm[i];
        snprintf(buf, sizeof(buf), "%s  cx=%4d  fm=%4d  tb=%4d",
                 LEG_NAMES[i], pwm.coxa, pwm.femur, pwm.tibia);
        Color lc = hud.gait_state.is_swing[i] ? Color{100,200,255,255} : Color{200,200,200,255};
        if (hud.options.direct_pwm_control_enabled && i == hud.selected_pwm_leg) {
            lc = Color{255, 220, 100, 255};
        }
        text(buf, lc, 13);
    }

    int left_panel_x = panel_x - 6;
    int left_panel_w = hud_width(left_lines, 6);
    stable_width = std::max(stable_width, left_panel_w);
    left_panel_w = stable_width;
    DrawRectangle(left_panel_x, left_panel_y, left_panel_w, py - left_panel_y + 8, {30, 30, 30, 180});
    for (const HudLine& line : left_lines) {
        DrawText(line.s.c_str(), line.x, line.y, line.size, line.c);
    }
}

void draw_right_panel(const HudContext& hud, int& stable_width)
{
    const bool controller_enabled = !hud.keyboard_enabled && !hud.wifi_enabled;
    const int right_lines = 16;
    int right_line_h = 19;
    int right_font = 16;
    int right_panel_h = 16 + right_lines * right_line_h;
    if (right_panel_h > hud.screen_height - 12) {
        right_line_h = std::max(12, (hud.screen_height - 20) / right_lines);
        right_font = std::max(10, right_line_h - 3);
    }

    int rx = 0;
    int ry = 12;
    int right_panel_y = 6;
    std::vector<HudLine> right_hud_lines;
    auto rtext = [&](const char* s, Color c = {200,200,200,255}) {
        right_hud_lines.push_back({s, c, rx, ry, right_font});
        ry += right_line_h;
    };

    char buf[256];
    rtext("CONTROLS", {150, 220, 150, 255});
    if (hud.options.direct_pwm_control_enabled) {
        if (hud.wifi_enabled) {
            rtext("Wi-Fi locomotion only");
            rtext("Use keyboard/gamepad");
            rtext("for direct PWM edits");
        } else if (controller_enabled && hud.input.device_available) {
            rtext("DPad L/R Leg");
            rtext("DPad U/D Joint");
            rtext("A/B   PWM +/-");
            rtext("L3    50us step");
            snprintf(buf, sizeof(buf), "X     Selected %d", config::PwmNeutral);
            rtext(buf);
            rtext("Y     Reset all");
        } else if (controller_enabled) {
            rtext("Connect controller");
        } else {
            rtext("1-6   Select leg");
            rtext("Left/Right Leg");
            rtext("Z/X   Joint prev/next");
            rtext("Tab   Next joint");
            rtext("Up/Dn PWM +/-");
            rtext("Ctrl  1us step");
            rtext("Shift 50us step");
            snprintf(buf, sizeof(buf), "R     Selected %d", config::PwmNeutral);
            rtext(buf);
            rtext("C     Reset all");
        }
    } else {
        if (hud.wifi_enabled) {
            rtext("Primary Y Walk");
            rtext("Primary X Turn");
            rtext("Secondary X Strafe");
            rtext("Height Field Up/Dn");
            rtext("Relay Field Status");
            rtext("STOP  Center sticks");
        } else if (controller_enabled && hud.input.device_available) {
            rtext("LStick Move/turn");
            rtext("RStick Strafe");
            rtext("LB/RB Height Dn/Up");
            rtext("LT/RT Speed Dn/Up");
            rtext("DPad Up Dance F/B");
            rtext("DPad L/R Dance S/C");
            rtext("L3    Creep mode");
            rtext("A     Cycle gait");
            rtext("X/Y/B Gaits 1/2/3");
        } else if (controller_enabled) {
            rtext("Connect controller");
        } else {
            rtext("W/S   Forward/Back");
            rtext("A/D   Strafe L/R");
            rtext("Q/E   Spin CCW/CW");
            rtext("R/F   Height Up/Dn");
            rtext("Up    Dance F/B");
            rtext("Left/Right Dance S/C");
            rtext("Shift Creep mode");
            rtext("1-4   Gait select");
            rtext("Tab   Cycle gait");
        }
    }
    rtext("RMB   Orbit camera");
    rtext("Scroll  Zoom");
    rtext("Esc   Quit");

    int right_panel_w = hud_width(right_hud_lines, 6);
    stable_width = std::max(stable_width, right_panel_w);
    right_panel_w = stable_width;
    rx = hud.screen_width - right_panel_w - 6;
    for (HudLine& line : right_hud_lines) line.x = rx;
    DrawRectangle(rx - 6, right_panel_y, right_panel_w, ry - right_panel_y + 8, {30, 30, 30, 160});
    for (const HudLine& line : right_hud_lines) {
        DrawText(line.s.c_str(), line.x, line.y, line.size, line.c);
    }
}

void draw_status_bar(const HudContext& hud)
{
    const bool controller_enabled = !hud.keyboard_enabled && !hud.wifi_enabled;
    char kbuf[512] = "Keys: ";
    if (hud.options.direct_pwm_control_enabled) {
        snprintf(kbuf, sizeof(kbuf), "%s PWM: %s %s  step=%dus",
                 hud.wifi_enabled
                    ? "Wi-Fi"
                    : controller_enabled
                    ? (hud.input.device_available
                        ? (hud.input.mapped_gamepad ? "Gamepad" : "Joystick")
                        : "Controller")
                    : "Keyboard",
                 LEG_NAMES[hud.selected_pwm_leg],
                 JOINT_NAMES[hud.selected_pwm_joint],
                 hud.ctrl ? 1 : (hud.shift ? 50 : 10));
    } else if (hud.wifi_enabled) {
        snprintf(kbuf, sizeof(kbuf),
                 "Wi-Fi: P(x=%+.2f y=%+.2f)  S(x=%+.2f y=%+.2f)  height=%.2f relay=%s  V=%.2f I=%.2f  updates=%d age=%.2fs",
                 hud.input.wifi_primary_x,
                 hud.input.wifi_primary_y,
                 hud.input.wifi_secondary_x,
                 hud.input.wifi_secondary_y,
                 hud.input.wifi_height,
                 hud.input.wifi_relay_status ? "on" : "off",
                 hud.input.wifi_voltage,
                 hud.input.wifi_current,
                 hud.input.wifi_update_count,
                 hud.input.wifi_seconds_since_update);
    } else if (controller_enabled && hud.input.device_available) {
        char down_buf[96] = "";
        int written = 0;
        for (int i = 0; i < hud.input.button_count && written < (int)sizeof(down_buf) - 8; i++) {
            if (hud.input.raw_buttons[i]) {
                written += snprintf(down_buf + written, sizeof(down_buf) - written,
                                    "%s%d", written == 0 ? "" : ",", i);
            }
        }
        snprintf(kbuf, sizeof(kbuf),
                 "%s: L(steer=%+.2f walk=%+.2f)  R(strafe=%+.2f)  LT/RT trim=%.2f/%.2f  walk=%.3f strf=%.3f spin=%.2f  axes=%d buttons=%d hats=%d down=%s%s%s%s",
                 hud.input.mapped_gamepad ? "Gamepad" : "Joystick",
                 -hud.input.left_x,
                 -hud.input.left_y,
                 -hud.input.right_x,
                 hud.input.left_trigger,
                 hud.input.right_trigger,
                 hud.current_walk_speed,
                 hud.current_strafe_speed,
                 hud.current_spin_rate,
                 hud.input.axis_count,
                 hud.input.button_count,
                 hud.input.hat_count,
                 written == 0 ? "-" : down_buf,
                 hud.shift ? "  [L3 creep]" : "",
                 hud.active_dance != DanceMode::NONE ? "  dance=" : "",
                 hud.active_dance != DanceMode::NONE ? dance_mode_name(hud.active_dance) : "");
    } else if (controller_enabled) {
        snprintf(kbuf, sizeof(kbuf), "Controller: no device connected");
    } else {
        if (!(hud.kW||hud.kS||hud.kA||hud.kD||hud.kQ||hud.kE||hud.kR||hud.kF)) strcat(kbuf, "idle");
        if (hud.kW) strcat(kbuf, "W ");
        if (hud.kS) strcat(kbuf, "S ");
        if (hud.kA) strcat(kbuf, "A ");
        if (hud.kD) strcat(kbuf, "D ");
        if (hud.kQ) strcat(kbuf, "Q ");
        if (hud.kE) strcat(kbuf, "E ");
        if (hud.kR) strcat(kbuf, "R ");
        if (hud.kF) strcat(kbuf, "F ");
        if (hud.shift) strcat(kbuf, "[Shift] ");
        if (hud.active_dance != DanceMode::NONE) {
            strcat(kbuf, "dance=");
            strcat(kbuf, dance_mode_name(hud.active_dance));
            strcat(kbuf, " ");
        }
    }

    DrawRectangle(0, hud.screen_height - 28, hud.screen_width, 28, {20, 20, 20, 200});
    DrawText(kbuf, 10, hud.screen_height - 22, 16, {200, 255, 200, 255});

    char buf[32];
    snprintf(buf, sizeof(buf), "%d fps", GetFPS());
    DrawText(buf, hud.screen_width - 80, hud.screen_height - 22, 16, {180, 180, 180, 200});
}

} // namespace

void record_footprints(const Vec3 feet[6], const bool is_swing[6], float dt)
{
    for (auto& fp : g_footprints) fp.age += dt;
    while (!g_footprints.empty() && g_footprints.front().age > config::FootprintLifetime)
        g_footprints.pop_front();

    for (int i = 0; i < 6; i++) {
        if (g_prev_swing[i] && !is_swing[i]) {
            if ((int)g_footprints.size() < config::MaxFootprints)
                g_footprints.push_back({(float)feet[i].x, (float)feet[i].y, i, 0.0f});
        }
        g_prev_swing[i] = is_swing[i];
    }
}

void draw_scene(const RobotParams& params,
                const BasePose& final_pose,
                const LegPoints fk_pts[6],
                const Vec3 feet_world[6],
                const bool is_swing[6])
{
    draw_grid((float)final_pose.x, (float)final_pose.y, 1.5f, 0.05f);
    draw_footprints();
    draw_body(final_pose, params);

    for (int i = 0; i < 6; i++) {
        draw_leg(fk_pts[i], is_swing[i]);
        draw_foot_target(feet_world[i], is_swing[i]);
        draw_fk_error(fk_pts[i].pts[4], feet_world[i]);
    }

    float ax_len = 0.06f;
    Vector3 bp_rl = to_rl(final_pose.x, final_pose.y, final_pose.z);
    DrawLine3D(bp_rl,
               {bp_rl.x + ax_len*(float)std::cos(final_pose.yaw),
                bp_rl.y,
                bp_rl.z - ax_len*(float)std::sin(final_pose.yaw)}, RED);
    DrawLine3D(bp_rl,
               {bp_rl.x - ax_len*(float)std::sin(final_pose.yaw),
                bp_rl.y,
                bp_rl.z - ax_len*(float)std::cos(final_pose.yaw)}, GREEN);
    DrawLine3D(bp_rl, {bp_rl.x, bp_rl.y + ax_len, bp_rl.z}, BLUE);
}

void draw_hud(const HudContext& hud)
{
    static int left_panel_w_stable = 0;
    static int right_panel_w_stable = 0;

    draw_left_panel(hud, left_panel_w_stable);
    draw_right_panel(hud, right_panel_w_stable);
    draw_status_bar(hud);
}
