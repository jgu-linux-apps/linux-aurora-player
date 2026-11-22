#!/bin/sh
# Native build for the machine you run it on (amd64, arm64, ...):
#   sudo apt build-dep .   (or: sudo apt install build-essential debhelper cmake qt6-base-dev libgstreamer1.0-dev)
#   ./build-deb.sh
# Result: ../linux-aurora-player_0.6.0_<arch>.deb
set -e
dpkg-buildpackage -us -uc -b
