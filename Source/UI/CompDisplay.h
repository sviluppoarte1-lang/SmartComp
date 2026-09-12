#pragma once
#include <JuceHeader.h>
#include "../DSP/Compressor.h"

// Static in/out transfer curve with live operating point
class CompCurveComponent : public juce::Component, public juce::Timer
{
public:
    explicit CompCurveComponent(Compressor& c);
    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }
    Compressor& comp;
};

// Gain-reduction + input/output level meters
class CompMeterComponent : public juce::Component, public juce::Timer
{
public:
    explicit CompMeterComponent(Compressor& c);
    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }
    void drawBar(juce::Graphics& g, juce::Rectangle<float> area, float db,
                 float minDB, float maxDB, juce::Colour fill, bool inverted);
    Compressor& comp;
    float grPeakHold = 0.0f;
    int grHoldCounter = 0;
};
