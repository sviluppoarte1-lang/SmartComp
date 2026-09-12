#pragma once
#include <JuceHeader.h>

// Modern dark LookAndFeel for SmartEQ: sleek sliders, glowing knobs,
// rounded combo boxes and pill-style toggle switches.
// All drawing is ASCII-safe and uses only the default JUCE font.
class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel();
    ~ModernLookAndFeel() override = default;

    // Vertical gain faders
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    // Rotary freq / Q knobs
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Filter-type combo boxes
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    // ON pill switches
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernLookAndFeel)
};
