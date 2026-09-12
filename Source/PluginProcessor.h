#pragma once
#include <JuceHeader.h>
#include "DSP/Compressor.h"
#include "DSP/AutoLeveler.h"
#include "DSP/SongDynamics.h"
#include "Presets/CompPresetManager.h"
// Public build: no licensing - full version, always unlocked.

class SmartCompAudioProcessor : public juce::AudioProcessor
{
public:
    SmartCompAudioProcessor();
    ~SmartCompAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SmartComp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Compressor& getComp() { return comp; }
    AutoLeveler& getLeveler() { return leveler; }
    CompPresetManager& getPresetManager() { return presetManager; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- LEARN (one-shot, UI applies the result) ----
    struct LearnResult
    {
        bool ready = false;
        AutoLeveler::SourceClass cls = AutoLeveler::SourceClass::Unknown;
        float confidence = 0;
        AutoLeveler::AutoParams autoParams;
        double thresholdDB = -18;
        AutoLeveler::Features features;
    };
    void requestLearn();                 // UI: start 4 s capture
    bool isLearning() const;
    float learnProgress() const;
    LearnResult takeLearnResult();       // UI: call when isLearning() goes false->ready

    // ---- SONG map: bar-by-bar dynamics (learn whole song, follow it) ----
    SongDynamics& getSongDynamics() { return songDyn; }
    void clearSongMap();

    // ---- Live detection readout (UI) ----
    int getDetectedClass() const { return detectedClass.load(); }
    float getDetectedConfidence() const { return detectedConf.load(); }
    float getEffectiveThreshold() const { return effectiveThr.load(); }
    float getAvgGR() const { return avgGRmeter.load(); }
    AutoLeveler::Features getLevelerFeatures() { return leveler.getFeatures(); }

    // ---- Preset persistence ----
    void setLastPreset(const juce::String& name, const juce::String& category);
    juce::String getLastPresetName() const;
    juce::String getLastPresetCategory() const;

    // ---- Public build: no license keys here - 45-minute demo per session ----
    static constexpr double kDemoLimitSeconds = 45.0 * 60.0;
    bool isDemoExpired() const;
    double getDemoSecondsRemaining() const;

private:
    void syncCompParams(double sr, int numSamples); // APVTS -> comp (threshold overridden by AUTO)
    void applySongFollow(CompParams& p, int numSamples); // SONG map deltas on top

    Compressor comp;
    SafetyLimiter safety; // final brickwall: clipping impossible by construction
    AutoLeveler leveler;
    CompPresetManager presetManager;

    // Song-map state (audio thread)
    SongDynamics songDyn;
    float songThrDelta = 0.0f;
    float songRatioDelta = 0.0f;
    double lastPpq = -1.0;
    bool lastPpqValid = false;
    bool songWasPlaying = false;

    std::atomic<double> demoSecondsUsed { 0.0 }; // demo timer only, no secrets

    std::atomic<int> detectedClass { (int)AutoLeveler::SourceClass::Unknown };
    std::atomic<float> detectedConf { 0.0f };
    std::atomic<float> effectiveThr { -18.0f };
    std::atomic<float> avgGRmeter { 0.0f };
    int detectCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartCompAudioProcessor)
};
