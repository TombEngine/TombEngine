#!/usr/bin/env bash
# Installs what the CMake build of TombEngine needs on Ubuntu 24.04 (also under WSL): compilers, the development
# packages of SDL3, SPIRV-Cross, and the tools Ubuntu does not package: PowerShell 7 for the build events and DXC with
# SPIR-V support for the shaders. Run as root (sudo, or "wsl -u root" from Windows).

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

POWERSHELL_URL="https://github.com/PowerShell/PowerShell/releases/download/v7.6.9/powershell_7.6.9-1.deb_amd64.deb"
DXC_URL="https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz"

apt-get update -qq
apt-get install -y -qq build-essential cmake ninja-build git curl pkg-config spirv-cross \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libwayland-dev libdecor-0-dev libegl-dev libgl-dev libdrm-dev libgbm-dev libvulkan-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev libibus-1.0-dev > /dev/null

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

if ! command -v pwsh > /dev/null; then
    curl -sSfL -o "$WORK_DIR/powershell.deb" "$POWERSHELL_URL"
    apt-get install -y -qq "$WORK_DIR/powershell.deb" > /dev/null
fi

# The DXC release keeps its library next to the executable in a separate folder, so a wrapper sets the search path.
if ! command -v dxc > /dev/null; then
    curl -sSfL -o "$WORK_DIR/dxc.tar.gz" "$DXC_URL"
    rm -rf /opt/dxc
    mkdir -p /opt/dxc
    tar -xzf "$WORK_DIR/dxc.tar.gz" -C /opt/dxc
    DXC_BIN="$(find /opt/dxc -type f -name dxc -perm -u+x | head -1)"
    DXC_LIB="$(dirname "$(find /opt/dxc -name 'libdxcompiler.so*' | head -1)")"
    printf '#!/bin/sh\nLD_LIBRARY_PATH="%s${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "%s" "$@"\n' "$DXC_LIB" "$DXC_BIN" > /usr/local/bin/dxc
    chmod +x /usr/local/bin/dxc
fi

echo "PowerShell: $(pwsh --version)"
echo "DXC: $(dxc --version 2>&1 | head -1)"
echo "SPIRV-Cross: $(command -v spirv-cross)"
