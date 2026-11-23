#!/usr/bin/env bash
# Build ET Broadcaster.app (arm64, or universal with UNIVERSAL=1) and wrap it in a drag-to-Applications .dmg
# with the E.T.TATTOO background.  Runs on macOS (your M1, or the GitHub Actions macOS runner).
#   ./build-dmg.sh                    -> packaging-out/ET-Broadcaster-<ver>-arm64.dmg
#   UNIVERSAL=1 ./build-dmg.sh        -> ...-universal.dmg (arm64 + x86_64)
#   JUCE_DIR=~/JUCE ./build-dmg.sh    (default: clones JUCE 9.0.5 (same as the Linux verification build) into ./third_party/JUCE once)
# Unsigned (no $99/yr Apple Developer account): first launch = right-click the app -> Open (see README-MAC.md).
set -euo pipefail
cd "$(dirname "$0")"
[ "$(uname)" = Darwin ] || { echo "build-dmg.sh must run on macOS (see README-MAC.md for why Linux cross-builds are not used)"; exit 1; }
VER=$(sed -n 's/^project(ETBroadcaster VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
ARCHS=${UNIVERSAL:+"arm64;x86_64"}; ARCHS=${ARCHS:-arm64}; TAG=$([ -n "${UNIVERSAL:-}" ] && echo universal || echo arm64)
JUCE_DIR=${JUCE_DIR:-$PWD/third_party/JUCE}
[ -d "$JUCE_DIR" ] || git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git "$JUCE_DIR"
BLD=$PWD/cmake-mac-$TAG; OUT=$PWD/packaging-out; mkdir -p "$OUT"
cmake -S . -B "$BLD" -G Xcode -DETB_APP=ON -DETB_TESTS=ON -DJUCE_DIR="$JUCE_DIR" -DCMAKE_OSX_ARCHITECTURES="$ARCHS" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build "$BLD" --config Release --target ETBStudio scratch_tests -- -quiet
"$BLD/Release/scratch_tests" > "$OUT/scratch-tests-macos.txt" || { echo "engine tests failed"; cat "$OUT/scratch-tests-macos.txt"; exit 1; }
APP=$(find "$BLD" -path "*Release*" -name "ET Broadcaster.app" -maxdepth 6 | head -1)
[ -d "$APP" ] || { echo "app bundle not found"; exit 1; }
lipo -info "$APP/Contents/MacOS/ET Broadcaster"
# ad-hoc signature: not a Developer ID, but keeps the bundle internally consistent (required on Apple Silicon)
codesign --force --deep --sign - "$APP"
DMG="$OUT/ET-Broadcaster-$VER-$TAG.dmg"; rm -f "$DMG"
STAGE=$(mktemp -d); cp -R "$APP" "$STAGE/"
if command -v create-dmg >/dev/null; then
  # brew install create-dmg   (window 660x420, icons placed on the arrow of packaging/dmg-background.png)
  # create-dmg often exits 1 on headless runners (Finder "bless" / AppleEvents) even when the .dmg was written.
  create-dmg --volname "ET Broadcaster" --background packaging/dmg-background.png --window-pos 200 120 --window-size 660 420 \
    --icon-size 112 --icon "ET Broadcaster.app" 170 235 --hide-extension "ET Broadcaster.app" --app-drop-link 490 235 \
    --no-internet-enable "$DMG" "$STAGE" || true
fi
if [[ ! -f "$DMG" ]]; then
  # plain hdiutil fallback: same layout via AppleScript on a read-write image, then compressed
  ln -s /Applications "$STAGE/Applications"; mkdir "$STAGE/.background"; cp packaging/dmg-background.png "$STAGE/.background/bg.png"
  RW=$(mktemp -u).dmg; hdiutil create -quiet -srcfolder "$STAGE" -volname "ET Broadcaster" -fs HFS+ -format UDRW -size 400m "$RW"
  DEV=$(hdiutil attach -readwrite -noverify -noautoopen "$RW" | awk '/Apple_HFS/ {print $1}'); sleep 1
  osascript <<OSA || echo "(Finder layout skipped — no GUI session)"
tell application "Finder"
  tell disk "ET Broadcaster"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {200, 120, 860, 540}
    set opts to the icon view options of container window
    set arrangement of opts to not arranged
    set icon size of opts to 112
    set background picture of opts to file ".background:bg.png"
    set position of item "ET Broadcaster.app" of container window to {170, 235}
    set position of item "Applications" of container window to {490, 235}
    close
  end tell
end tell
OSA
  sync; hdiutil detach -quiet "$DEV"; hdiutil convert -quiet "$RW" -format UDZO -imagekey zlib-level=9 -o "$DMG"; rm -f "$RW"
fi
rm -rf "$STAGE"; shasum -a 256 "$DMG" | tee "$DMG.sha256"; echo "built $DMG"
