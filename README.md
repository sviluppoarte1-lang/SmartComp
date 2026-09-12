# SmartComp by Fear Escape — Leveling Compressor-Limiter VST3

**Feed-forward compressor + brickwall limiter with automatic dynamics regulation:
it hears where it is inserted, recognizes the instrument or voice, and drives itself.**

Compatible **Windows &amp; Linux** — VST3 + Standalone — Mixing, Mastering &amp; Broadcast.

Inspired by the classic bus-glue workflow (slow attack preserving transients,
program-dependent auto release, gentle ratios — our own design and curves, nothing copied),
plus one exclusive feature: the **AutoLeveler**.

---

## The AutoLeveler (our extra)

1. Insert SmartComp anywhere — vocal chain, drum bus, mix bus, master.
2. Pick a **Profile** (Kick … Vocal … Full Mix, or **Auto-Detect**) to help it.
3. Press **LEARN** while audio plays: 4 seconds of analysis
   (RMS/peak/crest, dynamic range, spectral centroid over 5 bands,
   zero-crossing rate, transient density), then it classifies the source
   (e.g. `Learned: Vocal (78%)`) and sets threshold *relative to your level*,
   ratio, attack, release, knee and sidechain filter.
4. Engage **AUTO**: the threshold rides your slow RMS with a GR-cap servo,
   so dynamics stay regulated hands-free while you mix.

The live readout always shows what it hears (`Hearing: Drums 64%`),
crest, dynamic range, effective threshold and average GR.

## Compressor engine

- Stereo-linked feed-forward detector, Peak/RMS blend knob
- Threshold −60…0 dB, Ratio 1…20 (:1), Attack 0.02…300 ms, Release 20…3000 ms
- **Auto Release**: program-dependent (deeper GR → slower release, SSL-style glue)
- Soft knee 0…12 dB, **Auto Makeup** (matches output loudness to input)
- Parallel **Mix**, **sidechain HPF** 20…500 Hz, Input/Output gain
- **Brickwall limiter mode** with adjustable ceiling (streaming-safe)
- Live transfer curve with GR shading + operating point, GR meter with
  peak-hold and scales, IN/OUT meters

## 6 historic circuit models (our own curves, nothing copied)

Selectable per band-independent **Circuit** switch, each with its own
detector behavior and saturation color on the wet path (dry stays clean):
- **Modern** — transparent and precise, no color
- **Vari-Mu** (Fairchild-style) — ratio grows progressively with depth,
  lazy release, tube warmth
- **Opto** (LA-2A-style) — gentle catch, two-stage release with a slow
  1.5 s tail, tube glow
- **FET** (1176-style) — ultra-fast attack down to 0.02 ms, hard knee, grit
- **VCA** (SSL/dbX-style) — punchy bus glue, transformer sheen
- **Diode** (Neve-style bridge) — smooth thick leveling, warm drive

Plus a **Drive** knob (0…24 dB) on top of each model's color floor.

## 57 presets for every instrument

- **Vocals (9)**: Lead Level, Warm Glue, Rap/Trap, Backing, Podcast,
  Choir Blend, Intimate Whisper, Audiobook, Trailer Voiceover
- **Drums (4)**: Bus Glue SSL-style, Kick, Snare, Room/Overheads
- **Strings (6)**: Violin Section, Viola Ensemble, Cello Solo,
  Double Bass Arco, String Tutti Glue, Pizzicato
- **Percussion (6)**: Toms, Hi-Hat, Shaker/Tambourine, Congas,
  Percussion Loop, Clap Stack
- **Mastering (9)**: Safety Net, Transparent Glue, VariMu Warm,
  Modern Loud, Vinyl Gentle (lathe-safe SCF), Classical Wide,
  EDM Punch, Broadcast Safe (integrated ceiling), Acoustic Organic
- **Bass, Guitars, Keys, Orchestra, Mix Bus, Limiters**
- **Vintage (8)**: Fairchild 670 Glue + Vocal, LA-2A Vocal + Bass,
  1176 Drum Crush + All-Buttons Smash, Diode Mix Glue, Zener Drum Smash
Last-used preset is remembered on reopen.

Clip safety by construction: a transparent -0.5 dBFS brickwall guards
every profile and circuit (including LEARN monitoring, where drive is
additionally capped), so the output can never clip.

## License

This repository is fully unlocked — no keys, no demo limits.

---

## Build

Requirements: CMake ≥3.22, C++17, Git.
Linux: `build-essential cmake libasound2-dev libfreetype6-dev libx11-dev
libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev
libfontconfig1-dev`. Windows: Visual Studio 2022 + CMake.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# VST3: build/SmartComp_artefacts/Release/VST3/SmartComp.vst3
./install_linux.sh   # -> ~/.vst3/ + ~/.local/bin/
```

DSP tests (no host needed, 30+ checks: gain computer, GR accuracy,
makeup match, limiter ceiling, classifier on synth sources, circuit
models — VariMu progression, Opto tail, FET speed, saturation THD —,
all 49 presets finite, song-dynamics learn/follow/persistence):
compile `Source/DSP/*.cpp` + `Presets` with `juce_core` +
`juce_audio_basics` (+ events/data-structures/graphics for presets) and run.

```
Source/
 ├─ DSP/Compressor.{h,cpp}      feed-forward comp + limiter + meters
 ├─ DSP/AutoLeveler.{h,cpp}     features + 9-class recognizer + auto mapping
 ├─ DSP/SongDynamics.{h,cpp}    full-song bar-by-bar dynamics map: learn + follow
 ├─ Presets/CompPresetManager   25 instrument presets + profiles
 ├─ UI/CompDisplay              transfer curve + GR/level meters
 ├─ UI/ModernLookAndFeel        shared Fear Escape styling
 ├─ PluginProcessor             21 params, LEARN/AUTO, song learn/follow
 └─ PluginEditor                knobs, preset/profile browser, song controls
```

Created by Fear Escape — 2026.
