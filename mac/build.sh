#!/bin/zsh
# Builds "SidetoneOff.app", installs it in /Applications and opens it.
set -euo pipefail

APP_NAME="SidetoneOff"
EXECUTABLE="SidetoneOff"
SRC="${0:A:h}"
APP="$SRC/build/$APP_NAME.app"

ICONSET="$SRC/build/AppIcon.iconset"

rm -rf "$APP" "$ICONSET"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
swiftc -O "$SRC/main.swift" -o "$APP/Contents/MacOS/$EXECUTABLE"
cp "$SRC/Info.plist" "$APP/Contents/Info.plist"
swift "$SRC/make-icon.swift" "$ICONSET"
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
codesign --force --sign - "$APP"

pkill -x "$EXECUTABLE" || true
rm -rf "/Applications/$APP_NAME.app"
ditto "$APP" "/Applications/$APP_NAME.app"
open "/Applications/$APP_NAME.app"
