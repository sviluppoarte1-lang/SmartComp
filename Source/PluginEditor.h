#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/CompDisplay.h"
#include "UI/ModernLookAndFeel.h"

class SmartCompAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    SmartCompAudioProcessorEditor(SmartCompAudioProcessor&);
    ~SmartCompAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SmartCompAudioProcessor& processor;

    // Shared modern styling (declared first: outlives all components)
    ModernLookAndFeel modernLF;

    std::unique_ptr<CompCurveComponent> curve;
    std::unique_ptr<CompMeterComponent> meters;

    juce::Label titleLabel;
    juce::Label categoryLabel, presetLabel, profileLabel, circuitLabel;
    juce::ComboBox categoryBox, presetBox, profileBox, circuitBox;
    juce::TextButton learnButton { "LEARN" };
    juce::TextButton songLearnButton { "SONG" };
    juce::TextButton songFollowButton { "FOLLOW" };
    juce::TextButton songClearButton { "CLR" };
    juce::ToggleButton autoLevelButton { "AUTO" };
    juce::ToggleButton limiterButton { "LIMIT" };
    juce::ToggleButton bypassButton { "BYPASS" };

    // 13 knobs in order: Thr Ratio Atk Rel Knee Makeup Mix SCF Det Ceil Drive In Out
    struct KnobCell : public juce::Component
    {
        juce::Slider knob { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        KnobCell(const juce::String& name, juce::Colour accent);
        void resized() override;
    };
    juce::OwnedArray<KnobCell> knobCells;
    juce::ToggleButton autoRelButton { "AUTO" };
    juce::ToggleButton autoMkButton { "AUTO" };

    juce::Label detLabel, featLabel;
    juce::Label statusLabel;

    // (no license bar in the public build: full version, always unlocked)

    // Attachments (knob order must match)
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;
    juce::AudioProcessorValueTreeState::ButtonAttachment* songLearnAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* songFollowAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* bypassAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* limiterAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* autoLevelAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* autoRelAttach = nullptr;
    juce::AudioProcessorValueTreeState::ButtonAttachment* autoMkAttach = nullptr;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment* profileAttach = nullptr;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment* circuitAttach = nullptr;

    void updatePresetBox();
    void applyPresetFromBox();
    void applyLearnResult(const SmartCompAudioProcessor::LearnResult& r);
    void setParam(const juce::String& id, float value);
    void drawSectionPanel(juce::Graphics& g, juce::Rectangle<int> area);

    // Section panels behind the knob row (computed in resized)
    juce::Rectangle<int> panelDyn, panelLvl, panelOut;
    // Section titles live in their own strip above the knobs: zero overlap
    juce::Label secDynLabel, secLvlLabel, secOutLabel;

    bool isInitializing = true;
    bool learning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartCompAudioProcessorEditor)
};
