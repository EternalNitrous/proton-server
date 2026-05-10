#include "servo.h"

#include "config.h"

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#else
    #include <cerrno>
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

Servo2040Client::~Servo2040Client()
{
    close();
}

bool Servo2040Client::open(const std::string& port)
{
    port_ = port;
#ifdef _WIN32
    std::string win_port = port;
    if (win_port.rfind("\\\\.\\", 0) != 0) {
        win_port = "\\\\.\\" + win_port;
    }
    handle_ = CreateFileA(win_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        status_ = "open failed";
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        status_ = "serial config read failed";
        close_handle_only();
        return false;
    }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(handle_, &dcb)) {
        status_ = "serial config failed";
        close_handle_only();
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 25;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    SetCommTimeouts(handle_, &timeouts);
#else
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        status_ = "open failed";
        return false;
    }

    termios tio = {};
    if (tcgetattr(fd_, &tio) != 0) {
        status_ = "serial config read failed";
        close_handle_only();
        return false;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        status_ = "serial config failed";
        close_handle_only();
        return false;
    }
#endif
    connected_ = true;
    if (!set_relay(true)) {
        status_ = "relay write failed";
        return false;
    }
    return true;
}

void Servo2040Client::close()
{
    if (handle_is_open()) {
        set_blocking_writes();
        bool relay_off_sent = false;
        for (int attempt = 0; attempt < 8; attempt++) {
            relay_off_sent = set_relay(false) || relay_off_sent;
            drain_output();
            sleep_ms(25);
        }
        if (relay_off_sent) {
            status_ = "relay off";
        } else {
            status_ = "relay off failed";
        }
    }
    close_handle_only();
}

bool Servo2040Client::send_pwm_values(const std::array<int, 18>& values_by_pin)
{
    if (!connected_) return false;
    bool ok = send_set(0, values_by_pin.data(), static_cast<uint8_t>(values_by_pin.size()));
    status_ = ok ? "streaming" : "write failed";
    return ok;
}

bool Servo2040Client::read_voltage(double& voltage)
{
    if (!connected_) return false;

    int raw = 0;
    bool ok = send_get(25, 1, &raw);
    if (!ok) {
        status_ = "voltage read failed";
        return false;
    }

    voltage = (double)raw / config::Servo2040VoltageScale;
    return true;
}

bool Servo2040Client::read_current(double& current)
{
    if (!connected_) return false;

    int raw = 0;
    bool ok = send_get(24, 1, &raw);
    if (!ok) {
        status_ = "current read failed";
        return false;
    }

    current = ((double)raw - config::Servo2040CurrentOffset)
            * config::Servo2040CurrentScale;
    return true;
}

bool Servo2040Client::set_relay(bool enabled)
{
    if (!connected_) return false;
    bool ok = send_relay(enabled);
    if (ok) {
        relay_enabled_ = enabled;
        status_ = enabled ? "relay on" : "relay off";
    } else {
        status_ = "relay write failed";
    }
    return ok;
}

bool Servo2040Client::send_relay(bool enabled)
{
    int v = enabled ? 1 : 0;
    bool ok = send_set(26, &v, 1);
    if (!ok) connected_ = false;
    return ok;
}

bool Servo2040Client::send_set(uint8_t pin_index, const int* values, uint8_t count)
{
    std::array<uint8_t, 3 + 18 * 2> bytes = {};
    bytes[0] = 0xD3;
    bytes[1] = pin_index;
    bytes[2] = count;
    for (uint8_t i = 0; i < count; i++) {
        int v = std::max(0, std::min(16383, values[i]));
        bytes[3 + i * 2] = static_cast<uint8_t>(v & 0x7F);
        bytes[4 + i * 2] = static_cast<uint8_t>((v >> 7) & 0x7F);
    }
    return write_all(bytes.data(), 3 + count * 2);
}

bool Servo2040Client::send_get(uint8_t pin_index, uint8_t count, int* values)
{
    if (count == 0 || count > 18) return false;

    flush_input();

    uint8_t request[3] = {0xC7, pin_index, count};
    if (!write_all(request, sizeof(request))) {
        return false;
    }

    std::array<uint8_t, 3 + 18 * 2> response = {};
    size_t len = 3 + (size_t)count * 2;
    if (!read_exact(response.data(), len)) {
        return false;
    }

    if (response[0] != 0xC7 || response[1] != pin_index || response[2] != count) {
        return false;
    }

    for (uint8_t i = 0; i < count; i++) {
        values[i] = response[3 + i * 2] | (response[4 + i * 2] << 7);
    }
    return true;
}

bool Servo2040Client::write_all(const uint8_t* data, size_t len)
{
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) return false;

    size_t sent = 0;
    int retries = 0;
    while (sent < len) {
        DWORD written = 0;
        BOOL ok = WriteFile(handle_, data + sent, static_cast<DWORD>(len - sent), &written, nullptr);
        if (ok && written > 0) {
            sent += static_cast<size_t>(written);
            retries = 0;
            continue;
        }
        if (++retries > 100) {
            connected_ = false;
            return false;
        }
        Sleep(1);
    }
    return true;
#else
    if (fd_ < 0) return false;

    size_t sent = 0;
    int retries = 0;
    while (sent < len) {
        ssize_t n = ::write(fd_, data + sent, len - sent);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            retries = 0;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (++retries > 100) {
                connected_ = false;
                return false;
            }
            usleep(1000);
            continue;
        }
        connected_ = false;
        return false;
    }
    return true;
#endif
}

bool Servo2040Client::read_exact(uint8_t* data, size_t len)
{
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) return false;

    size_t got = 0;
    int retries = 0;
    while (got < len) {
        DWORD read = 0;
        BOOL ok = ReadFile(handle_, data + got, static_cast<DWORD>(len - got), &read, nullptr);
        if (ok && read > 0) {
            got += static_cast<size_t>(read);
            retries = 0;
            continue;
        }
        if (++retries > 50) {
            return false;
        }
        Sleep(1);
    }
    return true;
#else
    if (fd_ < 0) return false;

    size_t got = 0;
    int retries = 0;
    while (got < len) {
        ssize_t n = ::read(fd_, data + got, len - got);
        if (n > 0) {
            got += static_cast<size_t>(n);
            retries = 0;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (++retries > 50) {
                return false;
            }
            usleep(1000);
            continue;
        }
        if (n == 0) {
            if (++retries > 50) {
                return false;
            }
            usleep(1000);
            continue;
        }
        connected_ = false;
        return false;
    }
    return true;
#endif
}

bool Servo2040Client::handle_is_open() const
{
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return fd_ >= 0;
#endif
}

void Servo2040Client::set_blocking_writes()
{
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) return;

    COMMTIMEOUTS timeouts = {};
    timeouts.WriteTotalTimeoutConstant = 250;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(handle_, &timeouts);
#else
    if (fd_ < 0) return;

    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

void Servo2040Client::flush_input()
{
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        PurgeComm(handle_, PURGE_RXCLEAR);
    }
#else
    if (fd_ >= 0) {
        tcflush(fd_, TCIFLUSH);
    }
#endif
}

void Servo2040Client::drain_output()
{
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(handle_);
    }
#else
    if (fd_ >= 0) {
        tcdrain(fd_);
    }
#endif
}

void Servo2040Client::sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void Servo2040Client::close_handle_only()
{
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
    connected_ = false;
    relay_enabled_ = false;
}

ServoAngles to_servo_angles(int leg_index, const LegJoints& joints)
{
    (void)leg_index;
    constexpr double R2D = 180.0 / M_PI;
    double raw_coxa  = joints.coxa  * R2D;
    double raw_femur = joints.femur * R2D;
    double raw_tibia = joints.tibia * R2D;

    return {
        config::ServoCoxaCenterDeg + raw_coxa,
        config::ServoFemurCenterDeg + raw_femur,
        config::ServoTibiaCenterDeg + raw_tibia
    };
}

int angle_to_pwm(double angle_deg)
{
    return static_cast<int>(
        config::PwmMin + (angle_deg / config::ServoAngleRangeDeg) * (config::PwmMax - config::PwmMin));
}

int clamp_pwm(int pwm)
{
    return std::max(config::PwmMin, std::min(config::PwmMax, pwm));
}

double pwm_to_angle(int pwm)
{
    return ((double)pwm - config::PwmMin) * config::ServoAngleRangeDeg / (config::PwmMax - config::PwmMin);
}

PWMValues to_pwm_values(const ServoAngles& angles)
{
    return {
        angle_to_pwm(angles.coxa),
        angle_to_pwm(angles.femur),
        angle_to_pwm(angles.tibia)
    };
}

LegJoints to_leg_joints(int leg_index, const ServoAngles& angles)
{
    (void)leg_index;
    constexpr double D2R = M_PI / 180.0;
    LegJoints joints;
    joints.coxa  = (angles.coxa - config::ServoCoxaCenterDeg) * D2R;
    joints.femur = (angles.femur - config::ServoFemurCenterDeg) * D2R;
    joints.tibia = (angles.tibia - config::ServoTibiaCenterDeg) * D2R;
    return joints;
}

ServoAngles to_servo_angles(const PWMValues& pwm)
{
    return {
        pwm_to_angle(pwm.coxa),
        pwm_to_angle(pwm.femur),
        pwm_to_angle(pwm.tibia)
    };
}

namespace {

int get_pwm_joint(const PWMValues& pwm, int joint)
{
    if (joint == 0) return pwm.coxa;
    if (joint == 1) return pwm.femur;
    return pwm.tibia;
}

void set_pwm_joint(PWMValues& pwm, int joint, int value)
{
    if (joint == 0) pwm.coxa = value;
    else if (joint == 1) pwm.femur = value;
    else pwm.tibia = value;
}

} // namespace

std::array<int, 18> pwm_by_servo2040_pin_unflipped(const std::array<PWMValues, 6>& leg_pwm)
{
    std::array<int, 18> by_pin = {};
    for (int pin = 0; pin < 18; pin++) {
        by_pin[pin] = get_pwm_joint(
            leg_pwm[config::Servo2040PinLeg[pin]],
            config::Servo2040PinJoint[pin]);
    }
    return by_pin;
}

std::array<int, 18> pwm_by_servo2040_pin_for_hardware(const std::array<PWMValues, 6>& leg_pwm)
{
    auto by_pin = pwm_by_servo2040_pin_unflipped(leg_pwm);
    for (int pin = 0; pin < 18; pin++) {
        if (config::Servo2040FlipForHardware[pin]) {
            by_pin[pin] = config::Servo2040FlipPwmSum - by_pin[pin];
        }
    }
    return by_pin;
}

std::array<PWMValues, 6> pwm_by_leg_from_servo2040_pin(const std::array<int, 18>& by_pin)
{
    std::array<PWMValues, 6> leg_pwm = {};
    for (int pin = 0; pin < 18; pin++) {
        set_pwm_joint(
            leg_pwm[config::Servo2040PinLeg[pin]],
            config::Servo2040PinJoint[pin],
            by_pin[pin]);
    }
    return leg_pwm;
}

RobotState robot_state_from_pwm(const std::array<PWMValues, 6>& leg_pwm)
{
    RobotState state;
    for (int i = 0; i < 6; i++) {
        state.legs[i] = to_leg_joints(i, to_servo_angles(leg_pwm[i]));
    }
    return state;
}

int& selected_pwm(PWMValues& pwm, int joint)
{
    if (joint == 0) return pwm.coxa;
    if (joint == 1) return pwm.femur;
    return pwm.tibia;
}

std::array<PWMValues, 6> pwm_from_robot_state(const RobotState& state)
{
    std::array<PWMValues, 6> pwm = {};
    for (int i = 0; i < 6; i++) {
        pwm[i] = to_pwm_values(to_servo_angles(i, state.legs[i]));
    }
    return pwm;
}
