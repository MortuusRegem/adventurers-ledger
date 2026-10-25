#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
version=1.1.1
arch="${1:-$(uname -m)}"
case "$arch" in arm64|x86_64) ;; *) echo "Unsupported architecture: $arch"; exit 1;; esac
: "${QT_ROOT_DIR:?Set QT_ROOT_DIR to the macOS Qt installation}"
app="build-macos/Adventurers Ledger.app"
stage="build-macos/dmg-$arch"
mkdir -p "$stage" dist
test ! -e "$stage/Adventurers Ledger.app"
ditto "$app" "$stage/Adventurers Ledger.app"
app="$stage/Adventurers Ledger.app"
# Add the app's own rendered icon and license texts before signing.
mkdir -p build-macos/ledger.iconset "$app/Contents/Resources/licenses"
icon="build-macos/Adventurers Ledger.app/Contents/MacOS/icon.png"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$icon" --out "build-macos/ledger.iconset/icon_${size}x${size}.png" >/dev/null
    twice=$((size * 2))
    sips -z "$twice" "$twice" "$icon" --out "build-macos/ledger.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns build-macos/ledger.iconset -o "$app/Contents/Resources/ledger.icns"
/usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string ledger.icns' "$app/Contents/Info.plist"
cp ATTRIBUTION.txt MAC-INSTALL.md "$app/Contents/Resources/"
cp licenses/*.txt "$app/Contents/Resources/licenses/"
# Test output is not part of the shipped application.
find "$app/Contents/MacOS" -type f \( -name '*.png' -o -name 'self-test-failure.txt' \) -delete
"$QT_ROOT_DIR/bin/macdeployqt" "$app" -always-overwrite -codesign=-
codesign --verify --deep --strict --verbose=2 "$app"
# Exercise the deployed bundle, not just the build linked against the installed Qt.
QT_QPA_PLATFORM=offscreen "$app/Contents/MacOS/Adventurers Ledger" --self-test
find "$app/Contents/MacOS" -type f \( -name '*.png' -o -name 'self-test-failure.txt' \) -delete
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
cp MAC-INSTALL.md "$stage/READ ME - Installing on Mac.txt"
ln -s /Applications "$stage/Applications"
hdiutil create -volname "Adventurers Ledger" -srcfolder "$stage" -ov -format UDZO "dist/Adventurers-Ledger-$version-macOS-$arch.dmg"
hdiutil verify "dist/Adventurers-Ledger-$version-macOS-$arch.dmg"
shasum -a 256 "dist/Adventurers-Ledger-$version-macOS-$arch.dmg" > "dist/Adventurers-Ledger-$version-macOS-$arch.dmg.sha256"
