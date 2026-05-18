#pragma once

#include "types.h"

#include <array>
#include <cstdint>
#include <string>

struct ServoAngles {
    double coxa;
    double femur;
    double tibia;
};

struct PWMValues {
    int coxa;
    int femur;
    int tibia;
};

class Servo2040Client {
public:
    Servo2040Client() = default;
    ~Servo2040Client();

    bool open(const std::string& port);
    void close();

    bool is_connected() const { return connected_; }
    const std::string& status() const { return status_; }
    const std::string& port() const { return port_; }

    bool send_pwm_values(const std::array<int, 18>& values_by_pin);
    bool read_voltage(double& voltage);
    bool read_current(double& current);
    bool set_relay(bool enabled);
    bool relay_enabled() const { return relay_enabled_; }

private:
    bool send_relay(bool enabled);
    bool send_set(uint8_t pin_index, const int* values, uint8_t count);
    bool send_get(uint8_t pin_index, uint8_t count, int* values);
    bool write_all(const uint8_t* data, size_t len);
    bool read_exact(uint8_t* data, size_t len);
    bool handle_is_open() const;
    void set_blocking_writes();
    void flush_input();
    void drain_output();
    void sleep_ms(int ms);
    void close_handle_only();

    std::string port_;
    std::string status_ = "disabled";
    bool connected_ = false;
    bool relay_enabled_ = false;
#ifdef _WIN32
    void* handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

ServoAngles to_servo_angles(int leg_index, const LegJoints& joints);
int angle_to_pwm(double angle_deg);
int clamp_pwm(int pwm);
double pwm_to_angle(int pwm);
PWMValues to_pwm_values(const ServoAngles& angles);
LegJoints to_leg_joints(int leg_index, const ServoAngles& angles);
ServoAngles to_servo_angles(const PWMValues& pwm);
std::array<int, 18> pwm_by_servo2040_pin_unflipped(const std::array<PWMValues, 6>& leg_pwm);
std::array<int, 18> pwm_by_servo2040_pin_for_hardware(const std::array<PWMValues, 6>& leg_pwm);
std::array<PWMValues, 6> pwm_by_leg_from_servo2040_pin(const std::array<int, 18>& by_pin);
RobotState robot_state_from_pwm(const std::array<PWMValues, 6>& leg_pwm);
int& selected_pwm(PWMValues& pwm, int joint);
std::array<PWMValues, 6> pwm_from_robot_state(const RobotState& state);
std::string discover_servo2040_port();
