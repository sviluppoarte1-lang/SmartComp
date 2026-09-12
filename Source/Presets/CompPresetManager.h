#pragma once
#include <JuceHeader.h>
#include "../DSP/Compressor.h"
#include "../DSP/AutoLeveler.h"

// Full compressor preset: every knob + profile + behavior flags
struct CompPreset
{
    juce::String name;
    juce::String category;
    juce::String description;
    CompParams params;
    int profile = 0;          // 0 = Auto-Detect, else SourceClass+1 (see AutoLeveler::classNames)
    bool autoLevel = false;   // engage continuous AUTO leveler on load
    bool autoRelease = true;
    bool autoMakeup = true;
    double mix = 1.0;
    int model = 0;            // historic circuit model (see CircuitModelInfo)
    double drive = 0.0;       // saturation drive dB
};

class CompPresetManager
{
public:
    CompPresetManager();

    int getNumPresets() const { return presets.size(); }
    const CompPreset& getPreset(int idx) const { return presets.getReference(idx); }
    juce::StringArray getPresetNames() const;
    juce::StringArray getCategories() const;
    juce::Array<CompPreset> getPresetsForCategory(const juce::String& cat) const;
    int findPresetByName(const juce::String& name) const;

private:
    juce::Array<CompPreset> presets;
    void buildPresets();
    void add(const juce::String& name, const juce::String& cat, const juce::String& desc,
             AutoLeveler::SourceClass profile, bool autoLevel,
             double thr, double ratio, double atk, double rel, bool autoRel,
             double knee, bool autoMk, double makeup, double mix,
             double scHPF, double detMix, bool limiter, double ceiling,
             int model, double drive);
};
