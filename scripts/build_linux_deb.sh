#!/bin/bash
# Script to build and package Orka for Ubuntu/Debian (.deb wrapper)
# Uses Rust/Cargo instead of CMake

set -e

VERSION="1.0.0"
DEB_DIR="build/orka_${VERSION}_amd64"

echo "╔══════════════════════════════════════════╗"
echo "║  Linux DEB Packager — ORKA (Rust)        ║"
echo "╚══════════════════════════════════════════╝"

# 1. Build with Cargo
echo "[1/4] Building Orka with cargo..."
cargo build --release

echo "[2/4] Preparing Debian package structure..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$DEB_DIR/usr/share/orka/ui"
mkdir -p "$DEB_DIR/DEBIAN"

# 2. Copy compiled binary
cp target/release/orka "$DEB_DIR/usr/bin/"

# 3. Copy .desktop file
cp packaging/linux/orka.desktop "$DEB_DIR/usr/share/applications/"

# 4. Copy icon
if [ -f "logo/Square150x150Logo.png" ]; then
    cp logo/Square150x150Logo.png "$DEB_DIR/usr/share/icons/hicolor/256x256/apps/orka.png"
fi

# 5. Copy UI files for Webview
if [ -d "src/ui/webapp/dist" ]; then
    cp -r src/ui/webapp/dist/* "$DEB_DIR/usr/share/orka/ui/"
else
    echo "Warning: src/ui/webapp/dist not found. Run: cd src/ui/webapp && npm install && npm run build"
fi

# 6. Copy tray icon alongside binary
if [ -f "logo/Square44x44Logo.png" ]; then
    cp logo/Square44x44Logo.png "$DEB_DIR/usr/share/orka/orka-icon.png"
fi

# 7. Create control file
cat <<EOF > "$DEB_DIR/DEBIAN/control"
Package: orka
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Perepelytsia Orka Technologies <orka@example.com>
Depends: libgtk-3-0, libwebkit2gtk-4.1-0 | libwebkit2gtk-4.0-37, libx11-6, xdotool
Description: Intelligent keyboard layout converter
 Supports EN/UK, EN/KO, and EN/HE pairs with Alt+Z hotkey and system tray.
 Built with Rust for maximum performance and safety.
EOF

# Ensure permissions
chmod -R 0755 "$DEB_DIR"

# 8. Build DEB
echo "[3/4] Building .deb package..."
dpkg-deb --build "$DEB_DIR"

echo "[4/4] Success! Package: build/orka_${VERSION}_amd64.deb"
echo "Install: sudo dpkg -i build/orka_${VERSION}_amd64.deb"
