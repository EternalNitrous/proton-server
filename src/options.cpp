#include "options.h"

#include <cstdio>

void print_usage(const char* exe)
{
    std::printf("Usage: %s [--config FILE] [--servo2040 PORT] [--pwm-control] [--port PORT]\n", exe);
    std::printf("\n");
    std::printf("Options:\n");
    std::printf("  --config FILE          Load robot dimensions, servos, and pin map from a .conf file.\n");
    std::printf("  --validate-config      Check the config file and exit without opening the simulator.\n");
    std::printf("  --servo2040 PORT        Stream HUD PWM values to a Servo2040 serial port.\n");
    std::printf("  --servo2040-port PORT   Same as --servo2040.\n");
    std::printf("  --pwm-control-servo2040 PORT\n");
    std::printf("                          Directly edit PWM values and stream them to Servo2040.\n");
    std::printf("  --servo2040-pwm-sim     Render the simulated robot from the Servo2040 PWM packet.\n");
    std::printf("  --pwm-sim               Same as --servo2040-pwm-sim.\n");
    std::printf("  --pwm-control           Skip gait/IK and directly edit simulated PWM values.\n");
    std::printf("  --port PORT             Serve the Wi-Fi controller page on this port (default 8080).\n");
    std::printf("  --help                  Show this help.\n");
}

AppOptions parse_options(int argc, char** argv)
{
    AppOptions options;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
        } else if (arg == "--config") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a config file path.\n", arg.c_str());
                options.show_help = true;
                options.parse_error = true;
                break;
            }
            options.config_path = argv[++i];
            options.config_path_explicit = true;
        } else if (arg == "--validate-config") {
            options.validate_config_only = true;
        } else if (arg == "--servo2040" || arg == "--servo2040-port"
                   || arg == "--pwm-control-servo2040") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a serial port path.\n", arg.c_str());
                options.show_help = true;
                options.parse_error = true;
                break;
            }
            options.servo2040_enabled = true;
            options.servo2040_port = argv[++i];
            if (arg == "--pwm-control-servo2040") {
                options.direct_pwm_control_enabled = true;
            }
        } else if (arg == "--servo2040-pwm-sim" || arg == "--pwm-sim") {
            options.servo2040_pwm_sim_enabled = true;
        } else if (arg == "--pwm-control") {
            options.direct_pwm_control_enabled = true;
        } else if (arg == "--port") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a port number.\n", arg.c_str());
                options.show_help = true;
                options.parse_error = true;
                break;
            }
            options.wifi_controller_port = std::stoi(argv[++i]);
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            options.show_help = true;
            options.parse_error = true;
            break;
        }
    }
    return options;
}
