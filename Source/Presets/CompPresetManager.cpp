#include "CompPresetManager.h"

CompPresetManager::CompPresetManager() { buildPresets(); }

juce::StringArray CompPresetManager::getPresetNames() const
{
    juce::StringArray s;
    for (auto& p : presets) s.add(p.name);
    return s;
}

juce::StringArray CompPresetManager::getCategories() const
{
    juce::StringArray cats;
    for (auto& p : presets) if (!cats.contains(p.category)) cats.add(p.category);
    return cats;
}

juce::Array<CompPreset> CompPresetManager::getPresetsForCategory(const juce::String& cat) const
{
    juce::Array<CompPreset> r;
    for (auto& p : presets) if (p.category == cat) r.add(p);
    return r;
}

int CompPresetManager::findPresetByName(const juce::String& name) const
{
    for (int i = 0; i < presets.size(); ++i) if (presets[i].name == name) return i;
    return -1;
}

void CompPresetManager::add(const juce::String& name, const juce::String& cat, const juce::String& desc,
                            AutoLeveler::SourceClass profile, bool autoLevel,
                            double thr, double ratio, double atk, double rel, bool autoRel,
                            double knee, bool autoMk, double makeup, double mix,
                            double scHPF, double detMix, bool limiter, double ceiling,
                            int model, double drive)
{
    CompPreset p;
    p.name = name; p.category = cat; p.description = desc;
    p.profile = (int)profile + 1; // classNames()[0] = Auto-Detect
    p.autoLevel = autoLevel;
    p.autoRelease = autoRel; p.autoMakeup = autoMk; p.mix = mix;
    p.params.thresholdDB = thr; p.params.ratio = ratio;
    p.params.attackMs = atk; p.params.releaseMs = rel;
    p.params.autoRelease = autoRel; p.params.kneeDB = knee;
    p.params.makeupDB = makeup; p.params.autoMakeup = autoMk;
    p.params.mix = mix; p.params.scHPF = scHPF; p.params.detectorMix = detMix;
    p.params.limiter = limiter; p.params.ceilingDB = ceiling;
    p.params.circuitModel = model; p.params.driveDB = drive;
    p.model = model; p.drive = drive;
    presets.add(p);
}

void CompPresetManager::buildPresets()
{
    using SC = AutoLeveler::SourceClass;
    using CM = CircuitModelInfo;
    // Columns: name | cat | desc | profile | autoLvl | thr | ratio | atk | rel | autoR | knee | autoM | mk | mix | scHP | det | lim | ceil | model | drive

    // === VOCALS (existing, now with circuit character) ===
    add("Lead Vocal Level",        "Vocals", "Smooth modern vocal: fast catch, auto glue, SCF keeps lows out",        SC::Vocal, true,  -18, 3.0, 3,  180, true,  4, true,  0, 1.0, 90, 0.5, false, -0.5, CM::Opto, 3);
    add("Warm Vocal Glue",         "Vocals", "Gentle 2:1 warmth for ballads and jazz voices",                          SC::Vocal, false, -20, 2.0, 10, 300, true,  8, true,  0, 1.0, 80, 0.6, false, -0.5, CM::VariMu, 5);
    add("Rap / Trap Vocal Punch",  "Vocals", "Aggressive upfront rap vocal, tight and present",                        SC::Vocal, true,  -16, 4.0, 1,  120, true,  3, true,  0, 1.0, 100, 0.4, false, -0.5, CM::FET, 6);
    add("Backing Vocals Sit",      "Vocals", "Tucks doubles and harmonies behind the lead",                            SC::Vocal, false, -20, 3.0, 5,  250, true,  6, true,  0, 1.0, 90, 0.5, false, -0.5, CM::Opto, 3);
    add("Podcast / Voice-Over",    "Vocals", "Broadcast-style leveling: steady intelligibility",                       SC::Vocal, true,  -22, 3.0, 5,  200, true,  4, true,  0, 1.0, 80, 0.6, false, -0.5, CM::Modern, 0);
    // === VOCALS (new: choir, intimate, spoken, trailer) ===
    add("Choir Blend",             "Vocals", "Blends choir sections into one voice, controls peaks softly",            SC::Vocal, false, -22, 2.0, 15, 400, true,  8, true,  0, 1.0, 70, 0.6, false, -0.5, CM::Opto, 3);
    add("Intimate Whisper",        "Vocals", "Lifts breathy whispers without harshness",                               SC::Vocal, true,  -30, 2.5, 5,  250, true,  6, true,  0, 1.0, 90, 0.6, false, -0.5, CM::VariMu, 4);
    add("Audiobook Chapter",       "Vocals", "Hour-long consistency for narrators, gentle and fatigue-free",           SC::Vocal, true,  -24, 2.5, 8,  300, true,  6, true,  0, 1.0, 80, 0.6, false, -0.5, CM::Opto, 2);
    add("Trailer Voiceover",       "Vocals", "Larger-than-life aggressive voiceover punch",                            SC::Vocal, false, -14, 5.0, 1,  120, true,  3, true,  0, 1.0, 80, 0.4, false, -0.5, CM::FET, 7);

    // === DRUMS (existing) ===
    add("Drum Bus Glue (SSL-style)", "Drums", "Classic bus glue: slow attack, auto release, 2-4 dB of movement",      SC::Drums, false, -14, 2.0, 15, 300, true,  8, true,  0, 1.0, 30, 0.4, false, -0.5, CM::VCA, 2);
    add("Kick Punch",              "Drums", "Weight with click: lets the beater through",                             SC::Kick,  false, -16, 4.0, 2,  120, true,  4, true,  0, 1.0, 40, 0.3, false, -0.5, CM::FET, 5);
    add("Snare Crack",             "Drums", "Body plus snap, controls ring without killing it",                       SC::Snare, false, -16, 4.0, 1,  100, true,  3, true,  0, 1.0, 80, 0.3, false, -0.5, CM::FET, 5);
    add("Room / Overheads Glue",   "Drums", "Glues room mics and overheads into one kit",                             SC::Drums, false, -18, 3.0, 10, 250, true,  6, true,  0, 1.0, 40, 0.5, false, -0.5, CM::VCA, 2);

    // === BASS (existing) ===
    add("Electric Bass Even",      "Bass",  "Evens out fingerstyle dynamics, keeps the note",                         SC::Bass,  true,  -18, 4.0, 10, 250, true,  6, true,  0, 1.0, 30, 0.6, false, -0.5, CM::Opto, 3);
    add("Synth Bass Tight",        "Bass",  "Tightens sub and mid-bass for electronic mixes",                          SC::Bass,  false, -16, 4.0, 5,  180, true,  4, true,  0, 1.0, 30, 0.5, false, -0.5, CM::FET, 4);

    // === GUITARS (existing) ===
    add("Acoustic Strum Control",  "Guitars", "Tames strums, keeps sparkle and rhythm",                               SC::Guitar, true, -18, 3.0, 5,  200, true,  6, true,  0, 1.0, 80, 0.5, false, -0.5, CM::Opto, 3);
    add("Electric Rhythm Glue",    "Guitars", "Sits rhythm guitars in a dense rock mix",                              SC::Guitar, false, -16, 4.0, 5,  180, true,  4, true,  0, 1.0, 80, 0.4, false, -0.5, CM::FET, 4);
    add("Lead Guitar Sustain",     "Guitars", "Adds sustain and forwardness to solos",                                SC::Guitar, false, -20, 3.0, 10, 300, true,  6, true,  0, 1.0, 80, 0.5, false, -0.5, CM::VariMu, 5);

    // === KEYS (existing) ===
    add("Piano Gentle",            "Keys",  "Transparent piano control, preserves hammer attack",                     SC::Keys,  false, -20, 2.5, 10, 300, true,  8, true,  0, 1.0, 30, 0.5, false, -0.5, CM::Opto, 2);
    add("Synth / Keys Glue",       "Keys",  "Bonds layered keys and pads",                                            SC::Keys,  false, -18, 2.5, 15, 350, true,  8, true,  0, 1.0, 30, 0.6, false, -0.5, CM::VCA, 2);

    // === ORCHESTRA (existing) ===
    add("Strings Smooth",          "Orchestra", "Slow silky leveling for sections and pads",                          SC::Strings, false, -22, 2.0, 25, 500, true, 10, true,  0, 1.0, 40, 0.7, false, -0.5, CM::VariMu, 4);
    add("Brass Tame",              "Orchestra", "Controls brass stabs without dulling",                               SC::Strings, false, -16, 3.0, 5,  200, true,  6, true,  0, 1.0, 60, 0.4, false, -0.5, CM::FET, 4);

    // === STRINGS - full section (new) ===
    add("Violin Section",          "Strings", "Smooths violin I/II bow motion, keeps rosin detail",                  SC::Strings, false, -22, 2.0, 15, 400, true,  8, true,  0, 1.0, 60, 0.6, false, -0.5, CM::Opto, 3);
    add("Viola Ensemble",          "Strings", "Warms the inner voices without mud",                                   SC::Strings, false, -22, 2.5, 12, 350, true,  8, true,  0, 1.0, 50, 0.6, false, -0.5, CM::Opto, 3);
    add("Cello Solo",              "Strings", "Spot-mic cello: body control with singing sustain",                    SC::Strings, false, -20, 3.0, 10, 300, true,  6, true,  0, 1.0, 40, 0.5, false, -0.5, CM::VariMu, 5);
    add("Double Bass Arco",        "Strings", "Evens bowed bass, tames wolf tones gently",                            SC::Bass,    false, -20, 3.0, 15, 400, true,  8, true,  0, 1.0, 30, 0.6, false, -0.5, CM::VariMu, 4);
    add("String Tutti Glue",       "Strings", "Bonds the whole section into one ensemble",                            SC::Strings, false, -18, 2.0, 20, 450, true, 10, true,  0, 1.0, 40, 0.6, false, -0.5, CM::VariMu, 4);
    add("Pizzicato Control",       "Strings", "Catches pluck attacks, lets them ring",                                SC::Strings, false, -18, 4.0, 2,  150, true,  4, true,  0, 1.0, 60, 0.4, false, -0.5, CM::FET, 4);

    // === PERCUSSION (new) ===
    add("Toms Punch",              "Percussion", "Floor and rack toms: attack plus resonance control",                SC::Drums, false, -16, 4.0, 3,  150, true,  4, true,  0, 1.0, 50, 0.3, false, -0.5, CM::FET, 5);
    add("Hi-Hat Control",          "Percussion", "Sits hats in the groove without harshness",                         SC::Drums, false, -20, 3.0, 1,  100, true,  3, true,  0, 1.0, 200, 0.3, false, -0.5, CM::VCA, 2);
    add("Shaker / Tambourine",     "Percussion", "Steadies high percussion, keeps the shimmer",                       SC::Drums, false, -22, 2.5, 2,  120, true,  4, true,  0, 1.0, 300, 0.4, false, -0.5, CM::VCA, 2);
    add("Congas & Hand Percussion","Percussion", "Round conga tone, controlled slap",                                 SC::Drums, false, -18, 3.0, 5,  200, true,  5, true,  0, 1.0, 90, 0.4, false, -0.5, CM::Opto, 3);
    add("Percussion Loop Glue",    "Percussion", "Bonds sampled loops into the track",                                SC::Drums, false, -18, 2.5, 10, 250, true,  6, true,  0, 1.0, 60, 0.5, false, -0.5, CM::VCA, 2);
    add("Clap Stack",              "Percussion", "Thick layered claps with snap",                                     SC::Snare, false, -16, 4.0, 1,  120, true,  3, true,  0, 1.0, 120, 0.3, false, -0.5, CM::FET, 5);

    // === MIX BUS (existing) ===
    add("Mix Bus Glue",            "Mix Bus", "The glue: 2:1, slow attack, auto release, 2-4 dB GR",                  SC::Mix,   false, -14, 2.0, 15, 300, true,  8, true,  0, 1.0, 30, 0.4, false, -0.5, CM::VCA, 2);
    add("Mix Bus Gentle",          "Mix Bus", "Barely-there bus control for acoustic and jazz",                       SC::Mix,   false, -18, 1.5, 30, 600, true, 10, true,  0, 1.0, 30, 0.6, false, -0.5, CM::VariMu, 3);
    add("Parallel Crush (Mix 50%)","Mix Bus", "New-York style parallel smash blended at 50%",                        SC::Drums, false, -22, 6.0, 1,  100, false, 3, false, 6, 0.5, 60, 0.3, false, -0.5, CM::FET, 8);

    // === MASTERING (safety net + 8 program presets) ===
    add("Mastering Safety Net",    "Mastering", "1-2 dB of transparent mastering glue before the limiter",            SC::Mix,   false, -12, 1.5, 30, 600, true, 10, true,  0, 1.0, 30, 0.7, false, -0.5, CM::VariMu, 2);
    add("Mastering Transparent Glue", "Mastering", "Clean VCA bus glue for modern masters, 2-3 dB GR",                SC::Mix,   false, -14, 2.0, 20, 400, true,  8, true,  0, 1.0, 30, 0.5, false, -0.5, CM::VCA, 1);
    add("Mastering VariMu Warm",   "Mastering", "Fairchild-style warmth and density for the 2-bus",                   SC::Mix,   false, -14, 1.8, 15, 500, true,  8, true,  0, 1.0, 30, 0.6, false, -0.5, CM::VariMu, 4);
    add("Mastering Modern Loud",   "Mastering", "Competitive loudness glue, still breathing",                         SC::Mix,   false, -12, 3.0, 8,  250, true,  6, true,  0, 1.0, 30, 0.4, false, -0.5, CM::VCA, 2);
    add("Mastering Vinyl Gentle",  "Mastering", "Lathe-safe: high SCF keeps sub pumping out of the groove",           SC::Mix,   false, -14, 1.5, 25, 500, true, 10, true,  0, 1.0, 60, 0.6, false, -0.5, CM::VariMu, 2);
    add("Mastering Classical Wide","Mastering", "Barely-there control for orchestral and chamber masters",            SC::Strings, false, -16, 1.2, 50, 800, true, 12, true,  0, 1.0, 30, 0.7, false, -0.5, CM::Modern, 0);
    add("Mastering EDM Punch",     "Mastering", "Punchy electronic master glue with FET snap",                        SC::Mix,   false, -12, 3.0, 5,  200, true,  5, true,  0, 1.0, 30, 0.4, false, -0.5, CM::FET, 3);
    add("Mastering Broadcast Safe","Mastering", "TV/streaming-safe master with integrated ceiling",                  SC::Mix,   false, -14, 2.0, 10, 300, true,  6, true,  0, 1.0, 40, 0.5, true,  -1.0, CM::VCA, 1);
    add("Mastering Acoustic Organic", "Mastering", "Opto gentleness for folk, jazz and acoustic masters",             SC::Strings, false, -18, 2.0, 15, 400, true,  8, true,  0, 1.0, 40, 0.6, false, -0.5, CM::Opto, 2);
    add("Brickwall Limiter",       "Limiter", "Ceiling -0.5 dBFS: loud and clean for streaming",                      SC::Mix,   false, -6,  20.0, 1, 100, true,  0, false, 0, 1.0, 30, 0.2, true,  -0.5, CM::Modern, 0);
    add("Vocal Limiter (De-ess helper)", "Limiter", "Catches harsh vocal peaks before the final limiter",             SC::Vocal, false, -10, 12.0, 1, 80,  true,  0, false, 0, 1.0, 90, 0.2, true,  -1.0, CM::FET, 3);
    add("Drum Peak Limiter",       "Limiter", "Transparent peak control for drum bus",                                SC::Drums, false, -8,  20.0, 1, 120, true,  0, false, 0, 1.0, 40, 0.2, true,  -0.5, CM::FET, 2);

    // === VINTAGE CIRCUITS (new): the historic boxes, our own curves ===
    add("Fairchild 670 Mix Glue",  "Vintage", "Vari-mu bus magic: progressive ratio, lazy release, tube warmth",      SC::Mix,   false, -14, 2.0, 3,  800, false, 8, true,  0, 1.0, 30, 0.6, false, -0.5, CM::VariMu, 6);
    add("Fairchild Vocal",         "Vintage", "Silky vari-mu vocal: never harsh, always forward",                     SC::Vocal, false, -18, 2.5, 3,  400, false, 6, true,  0, 1.0, 80, 0.6, false, -0.5, CM::VariMu, 5);
    add("LA-2A Vocal Magic",       "Vintage", "Opto levelling: just set the peak screw and sing",                     SC::Vocal, true,  -18, 3.0, 10, 500, false, 4, true,  0, 1.0, 90, 0.7, false, -0.5, CM::Opto, 4);
    add("LA-2A Bass",              "Vintage", "The classic bass leveller: fat, even, round",                           SC::Bass,  true,  -18, 3.5, 10, 500, false, 5, true,  0, 1.0, 30, 0.7, false, -0.5, CM::Opto, 4);
    add("1176 Drum Crush",         "Vintage", "FET snap: aggressive, punchy, in-your-face drums",                     SC::Drums, false, -16, 8.0, 0.2, 120, false, 2, true,  0, 1.0, 50, 0.2, false, -0.5, CM::FET, 8);
    add("1176 All-Buttons Smash",  "Vintage", "The British mode: smashed room, explosive energy (blend with Mix)",    SC::Drums, false, -24, 20.0, 0.05, 80, false, 0, false, 4, 0.7, 60, 0.1, false, -0.5, CM::FET, 12);
    add("Diode Mix Glue",          "Vintage", "Neve-style bridge: thick smooth bus with warm drive",                  SC::Mix,   false, -15, 2.5, 8,  350, true,  7, true,  0, 1.0, 30, 0.5, false, -0.5, CM::Diode, 5);
    add("Zener Drum Smash",        "Vintage", "TG-style aggressive limiting for explosive drums",                     SC::Drums, false, -10, 12.0, 0.1, 100, false, 0, false, 0, 1.0, 40, 0.1, true,  -1.0, CM::FET, 9);

    // Total: Vocals 9, Drums 4, Bass 2, Guitars 3, Keys 2, Orchestra 2,
    //        Strings 6, Percussion 6, Mix Bus 3, Mastering 9, Limiter 3, Vintage 8 = 57
}
