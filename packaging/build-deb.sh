#!/bin/bash
#
# Build a Debian package (.deb) for the virus detector, then offer to install it.
#
# Produces one file: dist/virus-detector_<version>_<arch>.deb
# After building, the script asks whether to install it now. Answering "y" runs
# the command below; answering "n" (or in a non-interactive shell) just leaves
# the .deb in dist/ so you can copy it to another Debian/Ubuntu machine and
# install it there manually with:
#
#     sudo apt install ./dist/virus-detector_<version>_<arch>.deb
#
# Installed layout:
#     /usr/bin/av                               ← compiled binary
#     /opt/virus-detector/config/signatures.txt  ← signatures
#     /opt/virus-detector/config/exclude.txt     ← exclusions
#     /opt/virus-detector/runtime/               ← working dir
#     /lib/systemd/system/virus-detector.service ← runs scan-all on every boot
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

VERSION="${1:-0.1.0}"
ARCH="$(dpkg --print-architecture)"
STAGE="build/deb/virus-detector_${VERSION}_${ARCH}"
OUT="dist/virus-detector_${VERSION}_${ARCH}.deb"

echo ">> Compiling binary"
make >/dev/null

echo ">> Staging files"
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" "$STAGE/usr/bin" \
         "$STAGE/opt/virus-detector/config" \
         "$STAGE/opt/virus-detector/runtime" \
         "$STAGE/lib/systemd/system"

install -Dm755 av                    "$STAGE/usr/bin/av"
install -Dm644 config/signatures.txt "$STAGE/opt/virus-detector/config/signatures.txt"
install -Dm644 config/exclude.txt    "$STAGE/opt/virus-detector/config/exclude.txt"

cat > "$STAGE/lib/systemd/system/virus-detector.service" <<'EOF'
[Unit]
Description=Virus Detector Boot Scan
After=local-fs.target

[Service]
Type=oneshot
WorkingDirectory=/opt/virus-detector
ExecStart=/usr/bin/av scan-all

[Install]
WantedBy=multi-user.target
EOF

cat > "$STAGE/DEBIAN/control" <<EOF
Package: virus-detector
Version: $VERSION
Section: admin
Priority: optional
Architecture: $ARCH
Maintainer: Virus Detector Maintainers <root@localhost>
Depends: libc6, libstdc++6, libgcc-s1, libsqlite3-0
Description: Signature-based malware scanner with quarantine
 Scans the filesystem for known malware signatures, quarantines matches, and
 runs a full scan-all at every boot via a systemd service.
EOF

# Keep user edits to these across upgrades.
cat > "$STAGE/DEBIAN/conffiles" <<'EOF'
/opt/virus-detector/config/signatures.txt
/opt/virus-detector/config/exclude.txt
EOF

# Enable the boot scan on install, disable it on removal.
cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = configure ] && [ -d /run/systemd/system ]; then
    systemctl daemon-reload || true
    systemctl enable virus-detector.service || true
fi
EOF

cat > "$STAGE/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ -d /run/systemd/system ]; then
    systemctl disable --now virus-detector.service || true
fi
EOF

chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/prerm"

echo ">> Building $OUT"
mkdir -p dist
dpkg-deb --root-owner-group --build "$STAGE" "$OUT" >/dev/null

echo ">> Package created successfully:"
echo "   $OUT"
echo

# On EOF (no interactive terminal, e.g. CI or a pipe) read exits non-zero,
# which under `set -e` would abort the script. `|| answer=""` keeps things
# graceful: an empty answer falls through to the default "don't install" case.
read -r -p "Install Virus Detector now? [y/N] " answer || answer=""

case "$answer" in
    [yY]|[yY][eE][sS])
        echo ">> Installing Virus Detector..."
        # Use sudo only when not already root (some environments lack sudo).
        [ "$(id -u)" -eq 0 ] && SUDO="" || SUDO="sudo"
        $SUDO apt install -y "./$OUT"
        echo ">> Installation complete."
        echo ">> Virus Detector will run automatically on the next boot."
        ;;
    *)
        echo ">> Not installing."
        echo ">> Package is available at: $OUT"
        ;;
esac
