#!/bin/bash
# Fear Escape - SmartComp release packaging (.deb + AppImage)
# Run from the SmartComp-VST3 directory after a Release build.
set -e
cd "$(dirname "$0")"

VERSION="1.0.0"
ARCH="amd64"
NAME="SmartComp"
DISPLAY="SmartComp"
DESC="Leveling compressor-limiter with automatic dynamics regulation"
VST3_SRC="build/SmartComp_artefacts/Release/VST3/SmartComp.vst3"
STANDALONE_SRC="build/SmartComp_artefacts/Release/Standalone/SmartComp"
LOGO="../logo.jpg"
DIST="dist"
PKGDIR="$DIST/deb_${NAME,,}"

for f in "$VST3_SRC/Contents/x86_64-linux/SmartComp.so" "$STANDALONE_SRC"; do
  [ -e "$f" ] || { echo "MISSING: $f (build Release first)"; exit 1; }
done
command -v dpkg-deb >/dev/null || { echo "dpkg-deb missing"; exit 1; }

rm -rf "$DIST"
mkdir -p "$PKGDIR/DEBIAN" \
         "$PKGDIR/usr/lib/vst3" \
         "$PKGDIR/usr/bin" \
         "$PKGDIR/usr/share/applications" \
         "$PKGDIR/usr/share/icons/hicolor/256x256/apps" \
         "$PKGDIR/usr/share/doc/${NAME,,}"

# --- payload ---
cp -r "$VST3_SRC" "$PKGDIR/usr/lib/vst3/"
cp "$STANDALONE_SRC" "$PKGDIR/usr/bin/$NAME"
chmod 755 "$PKGDIR/usr/bin/$NAME"
find "$PKGDIR/usr/lib/vst3" -type f -name "*.so" -exec chmod 755 {} \;

# --- icon ---
if [ -f "$LOGO" ]; then
  convert "$LOGO" -gravity center -crop 720x720+0+0 +repage -resize 256x256 \
    "$PKGDIR/usr/share/icons/hicolor/256x256/apps/${NAME,,}.png"
fi

# --- desktop entry (standalone) ---
cat > "$PKGDIR/usr/share/applications/${NAME,,}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$DISPLAY
GenericName=Leveling Compressor
Comment=$DESC by Fear Escape
Exec=$NAME
Icon=${NAME,,}
Categories=AudioVideo;Audio;Music;
Terminal=false
StartupNotify=false
Keywords=compressor;limiter;VST;audiomixing;mastering;
EOF

# --- docs ---
cp README.md "$PKGDIR/usr/share/doc/${NAME,,}/README" 2>/dev/null || echo "$DISPLAY $VERSION by Fear Escape" > "$PKGDIR/usr/share/doc/${NAME,,}/README"
cat > "$PKGDIR/usr/share/doc/${NAME,,}/copyright" <<EOF
$DISPLAY $VERSION - Copyright (C) 2026 Fear Escape. All rights reserved.
Binary VST3 plugin + standalone application.
Built with JUCE 7 (GPLv3 / ISC dual-licensed components, see JUCE docs).
EOF

# --- control ---
INSTALLED_KB=$(du -sk "$PKGDIR/usr" | cut -f1)
cat > "$PKGDIR/DEBIAN/control" <<EOF
Package: ${NAME,,}
Version: $VERSION
Section: sound
Priority: optional
Architecture: $ARCH
Maintainer: Fear Escape
Installed-Size: $INSTALLED_KB
Depends: libc6, libstdc++6, libgcc-s1, libfreetype6, libx11-6, libxext6, libxcomposite1, libxcursor1, libxinerama1, libxrandr2, libfontconfig1, libasound2
Homepage: https://fearescape.example.com
Description: $DESC (VST3 + standalone)
 $DISPLAY by Fear Escape.
 .
 Installs the VST3 plugin to /usr/lib/vst3 and the standalone app to /usr/bin.
 45-minute demo per session; buy: https://www.paypal.com/paypalme/fearescape/19.99
EOF

fakeroot dpkg-deb --build "$PKGDIR" "$DIST/${NAME,,}_${VERSION}_${ARCH}.deb" 2>/dev/null \
  || dpkg-deb --build "$PKGDIR" "$DIST/${NAME,,}_${VERSION}_${ARCH}.deb"

echo ""
echo "=== AppImage (portable standalone + --install-vst3) ==="
if ! command -v appimagetool >/dev/null; then
  echo "appimagetool missing - skipping AppImage (deb is ready)"
else
  APPDIR="$DIST/AppDir_${NAME}"
  rm -rf "$APPDIR"
  mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib/vst3"
  cp "$STANDALONE_SRC" "$APPDIR/usr/bin/$NAME"
  cp -r "$VST3_SRC" "$APPDIR/usr/lib/vst3/"
  if [ -f "$PKGDIR/usr/share/icons/hicolor/256x256/apps/${NAME,,}.png" ]; then
    cp "$PKGDIR/usr/share/icons/hicolor/256x256/apps/${NAME,,}.png" "$APPDIR/${NAME,,}.png"
  fi
  cp "$PKGDIR/usr/share/applications/${NAME,,}.desktop" "$APPDIR/"
  cat > "$APPDIR/AppRun" <<'APPRUNEOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"

# Forward PulseAudio/PipeWire environment so ALSA "pulse" device works.
# This lets the standalone capture system audio (speakers / monitors).
[ -n "$PULSE_SERVER" ]        || PULSE_SERVER="unix:${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native"
[ -n "$XDG_RUNTIME_DIR" ]     || XDG_RUNTIME_DIR="/run/user/$(id -u)"
[ -n "$PULSE_SINK" ]          || PULSE_SINK="auto_null"
export PULSE_SERVER XDG_RUNTIME_DIR PULSE_SINK

case "$1" in
  --install-vst3)
    mkdir -p "$HOME/.vst3"
    cp -r "$HERE/usr/lib/vst3/SmartComp.vst3" "$HOME/.vst3/"
    echo "SmartComp VST3 installed to $HOME/.vst3/ - rescan plugins in your DAW."
    ;;
  --help|-h)
    echo "SmartComp (portable)"
    echo "  Run with no arguments : launch the standalone compressor"
    echo "  --install-vst3        : install the bundled VST3 into ~/.vst3"
    ;;
  *) exec "$HERE/usr/bin/SmartComp" "$@" ;;
esac
APPRUNEOF
  chmod +x "$APPDIR/AppRun"
  (cd "$DIST" && ARCH=x86_64 appimagetool "AppDir_${NAME}" "${NAME}-${VERSION}-x86_64.AppImage")
fi

echo ""
echo "=== dist/ ==="
ls -la "$DIST" | grep -vE "^totale|^d" || ls -la "$DIST"
