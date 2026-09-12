#!/bin/bash
set -e
echo "=== SmartComp Installer Linux (Fear Escape) ==="
echo "Leveling compressor-limiter with automatic dynamics regulation"

VST3_SRC="build/SmartComp_artefacts/Release/VST3/SmartComp.vst3"
VST3_DEST="$HOME/.vst3"
STANDALONE_SRC="build/SmartComp_artefacts/Release/Standalone/SmartComp"
BIN_DEST="$HOME/.local/bin"

if [ ! -d "$VST3_SRC" ]; then
  echo "VST3 non trovato. Esegui prima: cmake -B build && cmake --build build --config Release"
  exit 1
fi

mkdir -p "$VST3_DEST"
echo "Copia SmartComp in $VST3_DEST..."
cp -r "$VST3_SRC" "$VST3_DEST/"
chmod -R +r "$VST3_DEST/SmartComp.vst3" 2>/dev/null || true
echo "  ✓ VST3: $VST3_DEST/SmartComp.vst3"

if [ -f "$STANDALONE_SRC" ]; then
  mkdir -p "$BIN_DEST"
  cp "$STANDALONE_SRC" "$BIN_DEST/SmartComp"
  chmod +x "$BIN_DEST/SmartComp"
  echo "  ✓ Standalone: $BIN_DEST/SmartComp"
fi

echo ""
echo "SmartComp full version, fully unlocked."
echo "Riavvia la DAW e rescansiona i plugin VST3."
