#!/bin/bash
set -e

INSTALL_DIR=/opt/virus-detector

echo "Installing Virus Detector..."

# The binary is built by `make` at the project root as ./av. It is installed
# under the name av_scanner, which is what the service unit invokes.
sudo install -Dm755 av /usr/local/bin/av_scanner

# Fixed working directory so the program's relative paths (config/... and
# runtime/...) resolve the same way they do during development, even when
# systemd starts the service with cwd=/ at boot.
sudo mkdir -p "$INSTALL_DIR/config" "$INSTALL_DIR/runtime"
sudo cp config/signatures.txt "$INSTALL_DIR/config/signatures.txt"

# The service unit; daemon-reload makes systemd pick up the new file.
sudo cp virus-detector.service /etc/systemd/system/virus-detector.service
sudo systemctl daemon-reload

# Run automatically from the next boot onward.
sudo systemctl enable virus-detector.service

echo "Virus Detector installed."
echo "It will run scan-all automatically on boot."
