#!/bin/bash
# Script to build and package Orka for Ubuntu/Debian (.deb wrapper)

set -e

VERSION="1.0.0"
DEB_DIR="build/orka_${VERSION}_amd64"

echo "╔══════════════════════════════════════════╗"
echo "║  Linux DEB Packager — ORKA               ║"
echo "╚══════════════════════════════════════════╝"

# 1. Ensure it's built
./scripts/build_linux.sh

echo "[1/3] Preparing Debian package structure..."
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$DEB_DIR/DEBIAN"

# 2. Copy compiled binary
cp build/orka "$DEB_DIR/usr/bin/"

# 3. Create .desktop file
cat <<EOF > "$DEB_DIR/usr/share/applications/orka.desktop"
[Desktop Entry]
Name=Orka
Comment=Intelligent Keyboard Converter
Exec=/usr/bin/orka
Icon=orka
Terminal=false
Type=Application
Categories=Utility;
EOF

# 4. Copy icon (assuming imege.png is our icon)
if [ -f "imege.png" ]; then
    cp imege.png "$DEB_DIR/usr/share/icons/hicolor/256x256/apps/orka.png"
fi

# 4.5. Copy UI files for Webview
mkdir -p "$DEB_DIR/usr/share/orka/ui"
if [ -d "src/ui/webapp/dist" ]; then
    cp -r src/ui/webapp/dist/* "$DEB_DIR/usr/share/orka/ui/"
else
    echo "Warning: src/ui/webapp/dist not found. The GUI settings window will be empty."
fi

# 5. Create control file
cat <<EOF > "$DEB_DIR/DEBIAN/control"
Package: orka
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Perepelytsia Orka Technologies <orka@example.com>
Description: Intelligent keyboard layout converter
 Supports EN/UK, EN/KO, and EN/HE pairs with seamless overlay UI.
EOF

# Ensure permissions are correct for Debian packaging
chmod -R 0755 "$DEB_DIR"


# 6. Build DEB
echo "[2/3] Building .deb package..."
dpkg-deb --build "$DEB_DIR"

echo "[3/3] Success! Package is available at: build/orka_${VERSION}_amd64.deb"
echo "Install using: sudo dpkg -i build/orka_${VERSION}_amd64.deb"
