#include "ModernLookAndFeel.h"

ModernLookAndFeel::ModernLookAndFeel()
{
    // Base palette - dark professional theme
    setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a1d29));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff262b3d));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff00d4ff));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00d4ff));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff262b3d));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff14161f));
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a2e42));
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff00d4ff));

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1d29));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a2e42));
    setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff00d4ff));
    setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(0xff00d4ff));

    setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00ff88));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555a70));

    setColour(juce::Label::textColourId, juce::Colour(0xffaab0c5));
    setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff14161f));
}

juce::Font ModernLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(11.0f, juce::Font::plain);
}

juce::Font ModernLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    if (buttonHeight > 24) return juce::Font(14.0f, juce::Font::bold);
    return juce::Font(12.0f, juce::Font::bold);
}

juce::Font ModernLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(11.0f, juce::Font::plain);
}

// ---- Vertical gain fader: slim track + glow thumb + centre detent ----
void ModernLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                         const juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const bool isVertical = (style == juce::Slider::LinearVertical);
    const float trackThickness = 5.0f;
    juce::Rectangle<float> track;
    if (isVertical)
    {
        const float cx = x + width * 0.5f;
        track = { cx - trackThickness * 0.5f, (float) y + 4.0f, trackThickness, (float) height - 8.0f };
    }
    else
    {
        const float cy = y + height * 0.5f;
        track = { (float) x + 4.0f, cy - trackThickness * 0.5f, (float) width - 8.0f, trackThickness };
    }

    // Track background
    g.setColour(findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(track, trackThickness * 0.5f);

    // Fill from centre detent (0 dB) to thumb - bipolar fader look
    auto range = slider.getRange();
    const double centreVal = 0.0;
    const double proportionCentre = (centreVal - range.getStart()) / (range.getLength() != 0 ? range.getLength() : 1.0);
    const double proportionPos = slider.valueToProportionOfLength(slider.getValue());
    float fillStart, fillEnd;
    if (isVertical)
    {
        const float yCentre = track.getBottom() - (float) proportionCentre * track.getHeight();
        const float yPos = track.getBottom() - (float) proportionPos * track.getHeight();
        fillStart = juce::jmin(yCentre, yPos);
        fillEnd = juce::jmax(yCentre, yPos);
        juce::Rectangle<float> fill(track.getX(), fillStart, track.getWidth(), fillEnd - fillStart + 0.5f);
        juce::ColourGradient grad(juce::Colour(0xff00d4ff), fill.getCentreX(), fill.getY(),
                                  juce::Colour(0xff0088cc), fill.getCentreX(), fill.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fill, trackThickness * 0.5f);
        // Centre detent tick
        g.setColour(juce::Colour(0xff3a3f55));
        g.fillRect(track.getX() - 3.0f, yCentre - 0.75f, track.getWidth() + 6.0f, 1.5f);
    }
    else
    {
        const float xCentre = track.getX() + (float) proportionCentre * track.getWidth();
        const float xPos = track.getX() + (float) proportionPos * track.getWidth();
        fillStart = juce::jmin(xCentre, xPos);
        fillEnd = juce::jmax(xCentre, xPos);
        juce::Rectangle<float> fill(fillStart, track.getY(), fillEnd - fillStart + 0.5f, track.getHeight());
        g.setColour(findColour(juce::Slider::thumbColourId));
        g.fillRoundedRectangle(fill, trackThickness * 0.5f);
    }

    // Thumb: glowing circle
    const float thumbR = isVertical ? 9.0f : 8.0f;
    juce::Point<float> thumbCentre;
    if (isVertical)
        thumbCentre = { track.getCentreX(), juce::jlimit(track.getY(), track.getBottom(), sliderPos) };
    else
        thumbCentre = { juce::jlimit(track.getX(), track.getRight(), sliderPos), track.getCentreY() };

    // Glow
    g.setColour(juce::Colour(0x5500d4ff));
    g.fillEllipse(thumbCentre.x - thumbR - 3.0f, thumbCentre.y - thumbR - 3.0f,
                  (thumbR + 3.0f) * 2.0f, (thumbR + 3.0f) * 2.0f);
    // Body
    juce::ColourGradient body(juce::Colours::white, thumbCentre.x - 3, thumbCentre.y - 3,
                              findColour(juce::Slider::thumbColourId), thumbCentre.x + 4, thumbCentre.y + 4, true);
    g.setGradientFill(body);
    g.fillEllipse(thumbCentre.x - thumbR, thumbCentre.y - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour(juce::Colour(0xff0a0c12).withAlpha(0.6f));
    g.drawEllipse(thumbCentre.x - thumbR, thumbCentre.y - thumbR, thumbR * 2.0f, thumbR * 2.0f, 1.2f);
    // White core dot
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillEllipse(thumbCentre.x - 2.5f, thumbCentre.y - 2.5f, 5.0f, 5.0f);
}

// ---- Real-knob look: tick ring, metallic body, pointer cap ----
void ModernLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    const float minDim = juce::jmin((float) width, (float) height);
    const juce::Point<float> centre((float) x + width * 0.5f, (float) y + height * 0.5f);
    const float R = (minDim - 18.0f) * 0.5f; // body radius, room for the tick ring
    if (R < 8.0f) // too small: fall back to a simple knob
    {
        LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPosProportional,
                                         rotaryStartAngle, rotaryEndAngle, slider);
        return;
    }
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const juce::Colour accent = slider.findColour(juce::Slider::thumbColourId);

    // Tick ring (11 ticks, lit up to the current value)
    const int nTicks = 11;
    for (int i = 0; i < nTicks; ++i)
    {
        const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float) i / (float) (nTicks - 1);
        const bool active = a <= angle + 1e-4f;
        const bool major = (i == 0 || i == nTicks - 1 || i == nTicks / 2);
        const float r0 = R + 3.0f, r1 = R + (major ? 8.5f : 6.0f);
        const juce::Point<float> p0(centre.x + std::sin(a) * r0, centre.y - std::cos(a) * r0);
        const juce::Point<float> p1(centre.x + std::sin(a) * r1, centre.y - std::cos(a) * r1);
        g.setColour(active ? accent.withAlpha(0.95f) : juce::Colour(0xff3a4058));
        g.drawLine(juce::Line<float>(p0, p1), major ? 2.2f : 1.4f);
    }

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(centre.x - R + 2.0f, centre.y - R + 3.5f, R * 2.0f, R * 2.0f);

    // Metallic body: dark base + lighter top (brushed-metal feel)
    g.setColour(juce::Colour(0xff23273a));
    g.fillEllipse(centre.x - R, centre.y - R, R * 2.0f, R * 2.0f);
    {
        juce::ColourGradient metal(juce::Colour(0xff565e78), centre.x - R * 0.45f, centre.y - R * 0.55f,
                                   juce::Colour(0xff1a1d29), centre.x + R * 0.3f, centre.y + R * 0.5f, false);
        g.setGradientFill(metal);
        g.fillEllipse(centre.x - R, centre.y - R, R * 2.0f, R * 2.0f);
    }
    // Machined edge ring
    g.setColour(juce::Colour(0xff0b0d13));
    g.drawEllipse(centre.x - R, centre.y - R, R * 2.0f, R * 2.0f, 2.0f);
    g.setColour(juce::Colour(0xff4a5169));
    g.drawEllipse(centre.x - R + 1.2f, centre.y - R + 1.2f, (R - 1.2f) * 2.0f, (R - 1.2f) * 2.0f, 1.0f);
    // Top gloss crescent
    {
        juce::Path gloss;
        gloss.addCentredArc(centre.x, centre.y, R - 3.5f, R - 3.5f, 0.0f, -2.35f, -0.79f, true);
        g.setColour(juce::Colours::white.withAlpha(0.13f));
        g.strokePath(gloss, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Value arc (inside the edge, above the cap)
    const float arcR = R - 5.0f;
    {
        juce::Path rest;
        rest.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff10121a));
        g.strokePath(rest, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(accent.withAlpha(0.95f));
        g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Pointer cap
    const float capR = R * 0.60f;
    {
        juce::ColourGradient cap(juce::Colour(0xff2e3448), centre.x - capR * 0.4f, centre.y - capR * 0.5f,
                                 juce::Colour(0xff0e1016), centre.x + capR * 0.3f, centre.y + capR * 0.4f, false);
        g.setGradientFill(cap);
        g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour(juce::Colour(0xff454c63));
        g.drawEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    }
    // Pointer line from hub to cap edge
    {
        const float len = capR - 1.5f;
        const juce::Point<float> tip(centre.x + std::sin(angle) * len,
                                     centre.y - std::cos(angle) * len);
        g.setColour(juce::Colours::white);
        g.drawLine(juce::Line<float>(centre, tip), 2.4f);
        // Accent dot at the tip
        g.setColour(accent);
        g.fillEllipse(tip.x - 2.2f, tip.y - 2.2f, 4.4f, 4.4f);
    }
    // Hub
    g.setColour(juce::Colour(0xff0b0d13));
    g.fillEllipse(centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
}

// ---- Rounded dark combo box with cyan arrow ----
void ModernLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    juce::Rectangle<float> area(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    const float corner = 5.0f;
    {
        juce::ColourGradient bg(findColour(juce::ComboBox::backgroundColourId).brighter(0.08f),
                                0.0f, 0.0f,
                                findColour(juce::ComboBox::backgroundColourId).darker(0.12f),
                                0.0f, (float) height, false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(area, corner);
    }
    g.setColour(box.hasKeyboardFocus(true) ? findColour(juce::ComboBox::focusedOutlineColourId)
                                           : findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(area, corner, box.hasKeyboardFocus(true) ? 1.5f : 1.0f);

    // Arrow button zone
    juce::Rectangle<float> arrowZone((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    juce::Path arrow;
    const float cx = arrowZone.getCentreX();
    const float cy = arrowZone.getCentreY();
    arrow.startNewSubPath(cx - 4.0f, cy - 1.5f);
    arrow.lineTo(cx, cy + 2.5f);
    arrow.lineTo(cx + 4.0f, cy - 1.5f);
    g.setColour(findColour(juce::ComboBox::buttonColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

// ---- Pill-style ON/OFF switch with text label ----
void ModernLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool /*highlighted*/, bool /*down*/)
{
    const bool isOn = button.getToggleState();
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f, 4.0f);
    const float pillW = juce::jmin(36.0f, bounds.getWidth() * 0.45f);
    juce::Rectangle<float> pill(bounds.getX(), bounds.getY(), pillW, bounds.getHeight());
    const float corner = pill.getHeight() * 0.5f;

    g.setColour(isOn ? juce::Colour(0xff123a2a) : juce::Colour(0xff20242f));
    g.fillRoundedRectangle(pill, corner);
    g.setColour(isOn ? juce::Colour(0xff00ff88) : juce::Colour(0xff3a4058));
    g.drawRoundedRectangle(pill, corner, 1.2f);

    const float knobD = pill.getHeight() - 4.0f;
    const float knobX = isOn ? pill.getRight() - knobD - 2.0f : pill.getX() + 2.0f;
    juce::Rectangle<float> knob(knobX, pill.getY() + 2.0f, knobD, knobD);
    if (isOn)
    {
        g.setColour(juce::Colour(0x5500ff88));
        g.fillEllipse(knob.expanded(2.5f));
    }
    // ToggleButton::tickColourId lets each switch pick its own ON colour
    const juce::Colour onCol = button.findColour(juce::ToggleButton::tickColourId);
    g.setColour(isOn ? onCol : juce::Colour(0xff6a7186));
    g.fillEllipse(knob);
    g.setColour(juce::Colours::white.withAlpha(isOn ? 0.9f : 0.5f));
    g.fillEllipse(knob.getCentreX() - 1.5f, knob.getCentreY() - 1.5f, 3.0f, 3.0f);

    // Text label (was missing entirely - switches looked empty)
    auto textArea = bounds.withTrimmedLeft(pillW + 5.0f);
    g.setColour(isOn ? juce::Colours::white : juce::Colour(0xff8a93a6));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), textArea, juce::Justification::centredLeft, true);
}
