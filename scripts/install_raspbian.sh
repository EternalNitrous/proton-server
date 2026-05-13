#!/bin/sh
set -eu

SERVICE_NAME="proton-server"
BRANCH="headless"
APP_ARGS=""
SERVICE_USER="${SUDO_USER:-$(id -un)}"
START_SERVICE=1

usage() {
    cat <<EOF
Usage: $0 [main|headless] [options]

Installs Raspberry Pi OS / Raspbian dependencies, checks out the requested
branch, builds the simulator, creates the Proton Server hotspot, and enables
it at startup with systemd.

Options:
  --servo2040 PORT|auto         Pass a Servo2040 serial port or force autodiscovery.
  --auto-servo2040              Force Servo2040 autodiscovery.
  --no-servo2040                Disable Servo2040 output/autodiscovery.
  --servo2040-port PORT         Same as --servo2040.
  --pwm-control                 Start in direct PWM control mode.
  --pwm-control-servo2040 PORT  Start direct PWM control with Servo2040 output.
  --servo2040-pwm-sim           Mirror Servo2040 PWM packets into simulation.
  --pwm-sim                     Same as --servo2040-pwm-sim.
  --port PORT                   Wi-Fi controller port, default 8080.
  --service-user USER           Run the startup service as USER.
  --no-start                    Enable at boot, but do not start now.
  -h, --help                    Show this help.

Examples:
  $0 headless
  $0 main --port 8081
EOF
}

die() {
    echo "install_raspbian.sh: $*" >&2
    exit 1
}

append_arg() {
    case "$1" in
        *" "*) die "arguments with spaces are not supported: $1" ;;
    esac
    if [ -z "$APP_ARGS" ]; then
        APP_ARGS="$1"
    else
        APP_ARGS="$APP_ARGS $1"
    fi
}

if [ "$#" -gt 0 ]; then
    case "$1" in
        main|headless) BRANCH="$1"; shift ;;
        -h|--help) usage; exit 0 ;;
    esac
fi

while [ "$#" -gt 0 ]; do
    case "$1" in
        --servo2040|--servo2040-port|--pwm-control-servo2040|--port)
            [ "$#" -ge 2 ] || die "$1 requires a value"
            append_arg "$1"
            append_arg "$2"
            shift 2
            ;;
        --auto-servo2040|--no-servo2040|--no-servo2040-autodiscover|--pwm-control|--servo2040-pwm-sim|--pwm-sim)
            append_arg "$1"
            shift
            ;;
        --service-user)
            [ "$#" -ge 2 ] || die "$1 requires a value"
            SERVICE_USER="$2"
            shift 2
            ;;
        --no-start)
            START_SERVICE=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$REPO_DIR/build"
if [ "$BRANCH" = "headless" ]; then
    BUILD_DIR="$REPO_DIR/build-headless"
fi

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

run_repo() {
    if [ "$(id -u)" -eq 0 ] && [ "$SERVICE_USER" != "root" ]; then
        sudo -u "$SERVICE_USER" "$@"
    else
        "$@"
    fi
}

cd "$REPO_DIR"

[ -d .git ] || die "run this from inside a cloned proton-server git repo"
id "$SERVICE_USER" >/dev/null 2>&1 || die "service user does not exist: $SERVICE_USER"

echo "Installing build dependencies..."
$SUDO apt-get update
PACKAGES="ca-certificates git build-essential cmake pkg-config hostapd dnsmasq iptables iw wireless-tools network-manager"
if [ "$BRANCH" = "main" ]; then
    PACKAGES="$PACKAGES xorg-dev libasound2-dev mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev libudev-dev"
fi
$SUDO apt-get install -y $PACKAGES

echo "Checking out branch: $BRANCH"
if run_repo git show-ref --verify --quiet "refs/heads/$BRANCH"; then
    run_repo git checkout "$BRANCH"
elif run_repo git show-ref --verify --quiet "refs/remotes/origin/$BRANCH"; then
    run_repo git checkout -b "$BRANCH" "origin/$BRANCH"
else
    die "branch not found locally or at origin: $BRANCH"
fi

for group in dialout input video render; do
    if getent group "$group" >/dev/null 2>&1; then
        $SUDO usermod -a -G "$group" "$SERVICE_USER"
    fi
done

HOTSPOT_RUNNER="/usr/local/sbin/proton-hotspot-run"
HOTSPOT_SERVICE="/etc/systemd/system/proton-hotspot.service"
HOSTAPD_CONF="/etc/hostapd/proton-server.conf"
DNSMASQ_CONF="/etc/dnsmasq.d/proton-server.conf"
HOTSPOT_MARKER="/etc/proton-server/hotspot-enabled"

echo "Writing Proton Server hotspot configuration..."
$SUDO mkdir -p /etc/hostapd /etc/dnsmasq.d /etc/proton-server /usr/local/sbin
{
    echo "interface=uap0"
    echo "driver=nl80211"
    echo "ssid=Proton Server"
    echo "hw_mode=g"
    echo "channel=7"
    echo "wmm_enabled=1"
    echo "auth_algs=1"
    echo "ignore_broadcast_ssid=0"
} | $SUDO tee "$HOSTAPD_CONF" >/dev/null

{
    echo "interface=uap0"
    echo "bind-interfaces"
    echo "domain-needed"
    echo "bogus-priv"
    echo "dhcp-range=10.42.0.10,10.42.0.200,255.255.255.0,12h"
    echo "dhcp-option=3,10.42.0.1"
    echo "dhcp-option=6,1.1.1.1,8.8.8.8"
} | $SUDO tee "$DNSMASQ_CONF" >/dev/null

{
    echo "#!/bin/sh"
    echo "set -eu"
    echo
    echo "AP_IFACE=uap0"
    echo "UPSTREAM_IFACE=wlan0"
    echo "DNSMASQ_PID=/run/proton-dnsmasq.pid"
    echo
    echo "cleanup() {"
    echo "    if [ -f \"\$DNSMASQ_PID\" ]; then"
    echo "        kill \"\$(cat \"\$DNSMASQ_PID\")\" >/dev/null 2>&1 || true"
    echo "        rm -f \"\$DNSMASQ_PID\""
    echo "    fi"
    echo "    iptables -t nat -D POSTROUTING -o \"\$UPSTREAM_IFACE\" -j MASQUERADE >/dev/null 2>&1 || true"
    echo "    iptables -D FORWARD -i \"\$AP_IFACE\" -o \"\$UPSTREAM_IFACE\" -j ACCEPT >/dev/null 2>&1 || true"
    echo "    iptables -D FORWARD -i \"\$UPSTREAM_IFACE\" -o \"\$AP_IFACE\" -m state --state RELATED,ESTABLISHED -j ACCEPT >/dev/null 2>&1 || true"
    echo "    ip link set \"\$AP_IFACE\" down >/dev/null 2>&1 || true"
    echo "    iw dev \"\$AP_IFACE\" del >/dev/null 2>&1 || true"
    echo "}"
    echo
    echo "trap cleanup EXIT INT TERM"
    echo "cleanup"
    echo "iw dev \"\$UPSTREAM_IFACE\" interface add \"\$AP_IFACE\" type __ap"
    echo "ip addr add 10.42.0.1/24 dev \"\$AP_IFACE\""
    echo "ip link set \"\$AP_IFACE\" up"
    echo "sysctl -w net.ipv4.ip_forward=1 >/dev/null"
    echo "iptables -t nat -C POSTROUTING -o \"\$UPSTREAM_IFACE\" -j MASQUERADE >/dev/null 2>&1 || iptables -t nat -A POSTROUTING -o \"\$UPSTREAM_IFACE\" -j MASQUERADE"
    echo "iptables -C FORWARD -i \"\$AP_IFACE\" -o \"\$UPSTREAM_IFACE\" -j ACCEPT >/dev/null 2>&1 || iptables -A FORWARD -i \"\$AP_IFACE\" -o \"\$UPSTREAM_IFACE\" -j ACCEPT"
    echo "iptables -C FORWARD -i \"\$UPSTREAM_IFACE\" -o \"\$AP_IFACE\" -m state --state RELATED,ESTABLISHED -j ACCEPT >/dev/null 2>&1 || iptables -A FORWARD -i \"\$UPSTREAM_IFACE\" -o \"\$AP_IFACE\" -m state --state RELATED,ESTABLISHED -j ACCEPT"
    echo "dnsmasq --conf-file=$DNSMASQ_CONF --pid-file=\"\$DNSMASQ_PID\""
    echo "exec hostapd $HOSTAPD_CONF"
} | $SUDO tee "$HOTSPOT_RUNNER" >/dev/null
$SUDO chmod 755 "$HOTSPOT_RUNNER"

{
    echo "[Unit]"
    echo "Description=Proton Server Wi-Fi hotspot"
    echo "After=network.target"
    echo "Wants=network.target"
    echo
    echo "[Service]"
    echo "Type=simple"
    echo "ExecStart=$HOTSPOT_RUNNER"
    echo "Restart=on-failure"
    echo "RestartSec=3"
    echo
    echo "[Install]"
    echo "WantedBy=multi-user.target"
} | $SUDO tee "$HOTSPOT_SERVICE" >/dev/null
$SUDO touch "$HOTSPOT_MARKER"

echo "Building $BRANCH in $BUILD_DIR..."
run_repo cmake -S "$REPO_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
run_repo cmake --build "$BUILD_DIR" -j "$(getconf _NPROCESSORS_ONLN)"

EXECUTABLE="$BUILD_DIR/proton-server"
[ -x "$EXECUTABLE" ] || die "build finished, but executable was not found: $EXECUTABLE"

SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"
echo "Writing $SERVICE_FILE..."
{
    echo "[Unit]"
    echo "Description=Proton server ($BRANCH)"
    echo "After=network-online.target"
    echo "Wants=network-online.target"
    echo
    echo "[Service]"
    echo "Type=simple"
    echo "User=$SERVICE_USER"
    echo "WorkingDirectory=$REPO_DIR"
    echo "ExecStart=$EXECUTABLE $APP_ARGS"
    echo "Restart=on-failure"
    echo "RestartSec=3"
    echo "Environment=HEXAPOD_SIM_BRANCH=$BRANCH"
    if [ "$BRANCH" = "main" ]; then
        echo "Environment=DISPLAY=:0"
        echo "Environment=XAUTHORITY=/home/$SERVICE_USER/.Xauthority"
    fi
    echo
    echo "[Install]"
    echo "WantedBy=multi-user.target"
} | $SUDO tee "$SERVICE_FILE" >/dev/null

$SUDO systemctl daemon-reload
$SUDO systemctl unmask hostapd >/dev/null 2>&1 || true
$SUDO systemctl disable --now dnsmasq >/dev/null 2>&1 || true
$SUDO systemctl enable proton-hotspot.service
$SUDO systemctl enable "$SERVICE_NAME.service"

if [ "$START_SERVICE" -eq 1 ]; then
    $SUDO systemctl restart proton-hotspot.service
    $SUDO systemctl restart "$SERVICE_NAME.service"
fi

echo
echo "Installed $BRANCH build:"
echo "  $EXECUTABLE"
echo
echo "Startup service:"
echo "  $SERVICE_NAME.service"
echo "  proton-hotspot.service (SSID: Proton Server)"
echo
echo "Useful commands:"
echo "  sudo systemctl status $SERVICE_NAME.service"
echo "  sudo systemctl status proton-hotspot.service"
echo "  sudo journalctl -u $SERVICE_NAME.service -f"
if [ "$BRANCH" = "main" ]; then
    echo
    echo "Note: the main branch opens a local raylib window and expects Raspberry Pi OS Desktop/X on display :0."
    echo "Use the headless branch for Raspberry Pi OS Lite."
fi
