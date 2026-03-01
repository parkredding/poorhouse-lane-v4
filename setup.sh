#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# Poorhouse Lane Siren V4 — Setup Script
# Raspberry Pi Zero 2W Dub Siren Installer
# =============================================================================

# --- Color helpers -----------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${BLUE}[INFO]${NC}  $1"; }
success() { echo -e "${GREEN}[OK]${NC}    $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# --- Global variables --------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
IS_PI_ZERO=false
BOOT_CHANGED=false

# --- check_root() ------------------------------------------------------------
check_root() {
    if [[ "$EUID" -ne 0 ]]; then
        error "This script must be run as root (sudo bash setup.sh)"
        exit 1
    fi

    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}  Poorhouse Lane Siren V4 — Setup${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""

    # Detect real user (the one who ran sudo)
    REAL_USER="${SUDO_USER:-pi}"
    REAL_HOME=$(eval echo "~${REAL_USER}")
    INSTALL_DIR="${REAL_HOME}/dubsiren"

    info "Installing for user: ${REAL_USER}"
    info "Install directory:   ${INSTALL_DIR}"
    echo ""
}

# --- detect_pi() -------------------------------------------------------------
detect_pi() {
    if [[ -f /proc/device-tree/model ]]; then
        local model
        model=$(tr -d '\0' < /proc/device-tree/model)
        info "Detected: ${model}"
        if [[ "$model" == *"Zero"* ]]; then
            IS_PI_ZERO=true
        fi
    else
        warn "Not running on a Raspberry Pi."
        warn "Continuing anyway for development purposes."
    fi
}

# --- setup_swap() ------------------------------------------------------------
setup_swap() {
    if [[ "$IS_PI_ZERO" != true ]]; then
        info "Not a Pi Zero — skipping swap setup."
        return 0
    fi

    if [[ -f /swapfile ]] && grep -q '/swapfile' /proc/swaps 2>/dev/null; then
        success "Swap file already active."
        return 0
    fi

    info "Creating 1GB swap file (essential for Pi Zero builds)..."

    # Disable dphys-swapfile if present to avoid conflicts
    if systemctl is-active --quiet dphys-swapfile 2>/dev/null; then
        systemctl stop dphys-swapfile
        systemctl disable dphys-swapfile
    fi

    dd if=/dev/zero of=/swapfile bs=1M count=1024 status=progress
    chmod 600 /swapfile
    mkswap /swapfile
    swapon /swapfile

    # Make persistent across reboots
    if ! grep -q '/swapfile' /etc/fstab; then
        echo '/swapfile none swap sw 0 0' >> /etc/fstab
    fi

    success "1GB swap file created and activated."
}

# --- install_deps() ----------------------------------------------------------
install_deps() {
    info "Installing build dependencies..."
    apt-get update -qq
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        libasound2-dev \
        libgpiod-dev \
        gpiod
    success "Dependencies installed."
}

# --- build_rpi_ws281x() ------------------------------------------------------
build_rpi_ws281x() {
    if [[ -f /usr/local/lib/libws2811.so ]] || [[ -f /usr/local/lib/libws2811.a ]]; then
        success "rpi_ws281x already installed."
        return 0
    fi

    info "Building rpi_ws281x from source..."
    local build_dir
    build_dir=$(mktemp -d)

    git clone https://github.com/jgarff/rpi_ws281x.git "${build_dir}/rpi_ws281x"
    mkdir -p "${build_dir}/rpi_ws281x/build"
    cd "${build_dir}/rpi_ws281x/build"
    cmake -DBUILD_SHARED=ON -DBUILD_TEST=OFF ..

    # Use fewer jobs on Pi Zero to avoid OOM
    if [[ "$IS_PI_ZERO" == true ]]; then
        make -j2
    else
        make -j"$(nproc)"
    fi

    make install
    ldconfig

    # Clean up
    rm -rf "${build_dir}"
    cd "${SCRIPT_DIR}"

    success "rpi_ws281x installed."
}

# --- configure_boot() --------------------------------------------------------
configure_boot() {
    local boot_config=""
    if [[ -e /boot/firmware/config.txt ]]; then
        boot_config="/boot/firmware/config.txt"
    elif [[ -e /boot/config.txt ]]; then
        boot_config="/boot/config.txt"
    else
        warn "No boot config found. Skipping boot configuration."
        warn "If you're not on a Raspberry Pi, this is expected."
        return 0
    fi

    info "Configuring ${boot_config} for I2S DAC..."
    BOOT_CHANGED=false

    # Enable I2S
    if grep -q '^dtparam=i2s=on' "${boot_config}"; then
        success "I2S already enabled."
    elif grep -q '^#\s*dtparam=i2s=on' "${boot_config}"; then
        # Backup before first change
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        sed -i 's/^#\s*dtparam=i2s=on/dtparam=i2s=on/' "${boot_config}"
        BOOT_CHANGED=true
        info "Uncommented dtparam=i2s=on"
    else
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        echo 'dtparam=i2s=on' >> "${boot_config}"
        BOOT_CHANGED=true
        info "Added dtparam=i2s=on"
    fi

    # Add HiFiBerry DAC overlay
    if grep -q '^dtoverlay=hifiberry-dac' "${boot_config}"; then
        success "HiFiBerry DAC overlay already configured."
    else
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        echo 'dtoverlay=hifiberry-dac' >> "${boot_config}"
        BOOT_CHANGED=true
        info "Added dtoverlay=hifiberry-dac"
    fi

    # Disable onboard audio
    if grep -q '^dtparam=audio=on' "${boot_config}"; then
        if [[ "$BOOT_CHANGED" == false ]]; then
            cp "${boot_config}" "${boot_config}.bak.$(date +%Y%m%d%H%M%S)"
        fi
        sed -i 's/^dtparam=audio=on/#dtparam=audio=on/' "${boot_config}"
        BOOT_CHANGED=true
        info "Commented out dtparam=audio=on (disabled onboard audio)"
    else
        success "Onboard audio already disabled or not present."
    fi

    if [[ "$BOOT_CHANGED" == true ]]; then
        warn "Boot configuration changed. A reboot is required."
    else
        success "Boot configuration already up to date."
    fi
}

# --- configure_alsa() --------------------------------------------------------
configure_alsa() {
    info "Writing ALSA configuration for PCM5102 DAC..."

    cat > /etc/asound.conf << 'ALSA_EOF'
pcm.!default {
    type hw
    card 0
}

ctl.!default {
    type hw
    card 0
}
ALSA_EOF

    success "ALSA config written to /etc/asound.conf"
}

# --- create_mp3_dir() --------------------------------------------------------
create_mp3_dir() {
    mkdir -p "${INSTALL_DIR}/mp3s"
    chown -R "${REAL_USER}:${REAL_USER}" "${INSTALL_DIR}/mp3s"
    success "MP3 directory created at ${INSTALL_DIR}/mp3s"
}

# --- install_service() -------------------------------------------------------
install_service() {
    info "Installing systemd service..."

    cat > /etc/systemd/system/dubsiren.service << EOF
[Unit]
Description=Poorhouse Lane Dub Siren V4
After=sound.target

[Service]
Type=simple
ExecStart=${INSTALL_DIR}/build/dubsiren
WorkingDirectory=${INSTALL_DIR}
Restart=on-failure
RestartSec=3
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
Nice=-20
LimitRTPRIO=99
LimitMEMLOCK=infinity
User=root

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable dubsiren.service

    success "dubsiren.service installed and enabled."
    info "Service will not start until the binary is built."
}

# --- build_project() ---------------------------------------------------------
build_project() {
    info "Building the project..."

    cd "${SCRIPT_DIR}"
    mkdir -p build
    cd build
    cmake ..

    if [[ "$IS_PI_ZERO" == true ]]; then
        make -j2
    else
        make -j"$(nproc)"
    fi

    cd "${SCRIPT_DIR}"
    success "Build complete: ${SCRIPT_DIR}/build/dubsiren"
}

# --- print_summary() ---------------------------------------------------------
print_summary() {
    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}  Setup Complete!${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "  Install dir:  ${INSTALL_DIR}"
    echo "  Binary:       ${SCRIPT_DIR}/build/dubsiren"
    echo "  MP3 folder:   ${INSTALL_DIR}/mp3s/"
    echo "  Service:      dubsiren.service (enabled)"
    echo ""

    if [[ "$BOOT_CHANGED" == true ]]; then
        warn "Boot config was modified — a reboot is required!"
        echo ""
        read -rp "Reboot now? [y/N]: " reboot_choice
        if [[ "$reboot_choice" =~ ^[Yy]$ ]]; then
            info "Rebooting..."
            reboot
        else
            warn "Remember to reboot before using the dub siren!"
        fi
    fi
}

# =============================================================================
# Main
# =============================================================================
check_root
detect_pi
setup_swap
install_deps
build_rpi_ws281x
configure_boot
configure_alsa
create_mp3_dir
install_service
build_project
print_summary
