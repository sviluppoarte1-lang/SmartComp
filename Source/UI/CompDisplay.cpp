#include "CompDisplay.h"

CompCurveComponent::CompCurveComponent(Compressor& c) : comp(c) { startTimerHz(30); }

void CompCurveComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0d0d14));

    const float minDB = -60.f, maxInDB = 6.f;
    auto toX = [&](float inDB) {
        return bounds.getX() + (inDB - minDB) / (maxInDB - minDB) * bounds.getWidth();
    };
    auto toY = [&](float outDB) { // output range mirrors input, +12 headroom top
        return bounds.getBottom() - (outDB - minDB) / (maxInDB + 12.f - minDB) * bounds.getHeight();
    };

    // Grid
    g.setFont(9.f);
    for (int db = -60; db <= 0; db += 12)
    {
        float x = toX((float)db), y = toY((float)db);
        g.setColour(juce::Colour(0xff1e1e2a));
        g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        g.setColour(juce::Colour(0xff6a6a80));
        g.drawText(juce::String(db), (int)(x - 20), (int)(bounds.getBottom() - 13), 40, 11, juce::Justification::centred);
        g.drawText(juce::String(db), (int)(bounds.getX() + 3), (int)(y - 10), 34, 10, juce::Justification::left);
    }
    // Unity diagonal
    {
        juce::Path unity;
        unity.startNewSubPath(toX(minDB), toY(minDB));
        unity.lineTo(toX(maxInDB), toY(maxInDB));
        g.setColour(juce::Colour(0xff2a2a40));
        g.strokePath(unity, juce::PathStrokeType(1.0f));
    }

    const auto& p = comp.getParams();
    const float thr = (float)p.thresholdDB;

    // Circuit model tag, top-right
    {
        juce::String tag = juce::String(CircuitModelInfo::name(p.circuitModel)) + (p.limiter ? " LIMIT" : "");
        g.setFont(juce::Font(11.f, juce::Font::bold));
        const int tw = 130;
        juce::Rectangle<float> tagBg(bounds.getRight() - (float) tw - 8, bounds.getY() + 6, (float) tw, 18);
        g.setColour(juce::Colour(0xff0d0d14).withAlpha(0.7f));
        g.fillRoundedRectangle(tagBg, 4.0f);
        g.setColour(juce::Colour(0xffffcc00));
        g.drawText(tag, tagBg, juce::Justification::centred, false);
    }

    // Threshold line + flag with readable background
    g.setColour(juce::Colour(0xffff3b30).withAlpha(0.7f));
    float tx = toX(thr);
    g.drawVerticalLine((int)tx, bounds.getY(), bounds.getBottom());
    {
        g.setFont(10.f);
        juce::String flag = "THR " + juce::String(thr, 1);
        const int fw = 72;
        float fx = juce::jmin(tx + 3.0f, bounds.getRight() - (float) fw);
        juce::Rectangle<float> flagBg(fx - 2, bounds.getY() + 2, (float) fw, 14);
        g.setColour(juce::Colour(0xff0d0d14).withAlpha(0.75f));
        g.fillRoundedRectangle(flagBg, 3.0f);
        g.setColour(juce::Colour(0xffff3b30).withAlpha(0.9f));
        g.drawText(flag, flagBg, juce::Justification::centredLeft, false);
    }

    // Limiter ceiling
    if (p.limiter)
    {
        float cy = toY((float)p.ceilingDB);
        g.setColour(juce::Colour(0xffffcc00).withAlpha(0.8f));
        g.drawHorizontalLine((int)cy, bounds.getX(), bounds.getRight());
        g.drawText("CEIL " + juce::String(p.ceilingDB, 1), (int)(bounds.getRight() - 83), (int)(cy - 14), 80, 12, juce::Justification::right);
    }

    // Gain-reduction area between unity and the transfer curve (classic comp look)
    {
        juce::Path grFill;
        bool f2 = true;
        for (int i = 0; i <= 200; ++i)
        {
            float inDB = minDB + (maxInDB - minDB) * i / 200.f;
            float gr = Compressor::staticGainReduction(inDB, thr, (float)p.ratio, (float)p.kneeDB);
            float x = toX(inDB);
            if (f2) { grFill.startNewSubPath(x, toY(inDB)); grFill.lineTo(x, toY(inDB - gr)); f2 = false; }
            else grFill.lineTo(x, toY(inDB - gr));
        }
        for (int i = 200; i >= 0; --i)
        {
            float inDB = minDB + (maxInDB - minDB) * i / 200.f;
            grFill.lineTo(toX(inDB), toY(inDB));
        }
        grFill.closeSubPath();
        juce::ColourGradient fillGrad(juce::Colour(0x55ff3b30), bounds.getCentreX(), bounds.getY(),
                                      juce::Colour(0x08ff3b30), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(fillGrad);
        g.fillPath(grFill);
    }

    // Transfer curve (pre-makeup, like classic comp graphs)
    juce::Path curve;
    bool first = true;
    for (int i = 0; i <= 200; ++i)
    {
        float inDB = minDB + (maxInDB - minDB) * i / 200.f;
        float gr = Compressor::staticGainReduction(inDB, thr, (float)p.ratio, (float)p.kneeDB);
        float outDB = inDB - gr;
        float x = toX(inDB), y = toY(outDB);
        if (first) { curve.startNewSubPath(x, y); first = false; }
        else curve.lineTo(x, y);
    }
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.strokePath(curve, juce::PathStrokeType(6.f, juce::PathStrokeType::curved));
    g.setColour(juce::Colours::white);
    g.strokePath(curve, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved));

    // Knee region shading
    if (p.kneeDB > 0.05)
    {
        float kx0 = toX(thr - (float)p.kneeDB * 0.5f);
        float kx1 = toX(thr + (float)p.kneeDB * 0.5f);
        g.setColour(juce::Colour(0x2200d4ff));
        g.fillRect(juce::Rectangle<float>(kx0, bounds.getY(), kx1 - kx0, bounds.getHeight()));
    }

    // Live operating point with halo + crosshair
    {
        float inDB = juce::jlimit(minDB, maxInDB, comp.getInputLevelDB());
        float gr = comp.getGainReductionDB();
        float x = toX(inDB), y = toY(inDB - gr);
        g.setColour(juce::Colour(0x5500d4ff));
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
        g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        g.setColour(juce::Colour(0x3300d4ff));
        g.fillEllipse(x - 10, y - 10, 20, 20);
        g.setColour(juce::Colour(0xff00d4ff));
        g.fillEllipse(x - 6, y - 6, 12, 12);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawEllipse(x - 6, y - 6, 12, 12, 1.2f);
        g.setColour(juce::Colours::white);
        g.setFont(10.f);
        juce::String grText = juce::String(gr, 1) + " dB GR";
        float lx = juce::jmin(x + 9.0f, bounds.getRight() - 92.0f);
        juce::Rectangle<float> grBg(lx - 2, y - 20, 92, 14);
        g.setColour(juce::Colour(0xff0d0d14).withAlpha(0.75f));
        g.fillRoundedRectangle(grBg, 3.0f);
        g.setColour(juce::Colours::white);
        g.drawText(grText, grBg, juce::Justification::centredLeft, false);
    }

    g.setColour(juce::Colour(0xff1e1e2a));
    g.drawRect(bounds, 1.5f);
}

// ============================ meters ============================

CompMeterComponent::CompMeterComponent(Compressor& c) : comp(c) { startTimerHz(30); }

void CompMeterComponent::drawBar(juce::Graphics& g, juce::Rectangle<float> area, float db,
                                 float minDB, float maxDB, juce::Colour fill, bool inverted)
{
    g.setColour(juce::Colour(0xff1a1d29));
    g.fillRoundedRectangle(area, 4.0f);
    float t = juce::jlimit(0.f, 1.f, (db - minDB) / (maxDB - minDB));
    juce::Rectangle<float> fillR = area;
    if (!inverted) // level bars grow from bottom
    {
        float h = area.getHeight() * t;
        fillR = area.withY(area.getBottom() - h).withHeight(h);
    }
    else // GR bar grows from top
    {
        fillR = area.withHeight(area.getHeight() * t);
    }
    if (fillR.getHeight() > 0.5f)
    {
        juce::ColourGradient grad(fill.brighter(0.25f), fillR.getCentreX(), inverted ? fillR.getY() : fillR.getBottom(),
                                  fill.darker(0.25f), fillR.getCentreX(), inverted ? fillR.getBottom() : fillR.getY(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillR, 4.0f);
    }
}

void CompMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0d0d14));

    float gr = comp.getGainReductionDB();   // positive dB
    float inDB = comp.getInputLevelDB();
    float outDB = comp.getOutputLevelDB();

    // Peak hold for GR
    if (gr >= grPeakHold) { grPeakHold = gr; grHoldCounter = 45; }
    else if (--grHoldCounter <= 0) grPeakHold = juce::jmax(gr, grPeakHold - 0.25f);

    auto cols = bounds;
    cols.reduce(8, 8);
    float colW = (cols.getWidth() - 16.f) / 3.f;

    auto grArea = cols.removeFromLeft(colW);
    cols.removeFromLeft(8);
    auto inArea = cols.removeFromLeft(colW);
    cols.removeFromLeft(8);
    auto outArea = cols;

    g.setFont(10.f);
    g.setColour(juce::Colour(0xffaab0c5));
    g.drawText("GR", (int)grArea.getX(), (int)(grArea.getY() - 2), (int)grArea.getWidth(), 14, juce::Justification::centred);
    g.drawText("IN", (int)inArea.getX(), (int)(inArea.getY() - 2), (int)inArea.getWidth(), 14, juce::Justification::centred);
    g.drawText("OUT", (int)outArea.getX(), (int)(outArea.getY() - 2), (int)outArea.getWidth(), 14, juce::Justification::centred);

    auto bar = [](juce::Rectangle<float> a) { return a.withTrimmedTop(14); };
    drawBar(g, bar(grArea), gr, 0.f, 24.f, juce::Colour(0xffff3b30), true);
    drawBar(g, bar(inArea), inDB, -60.f, 3.f, juce::Colour(0xff00d4ff), false);
    drawBar(g, bar(outArea), outDB, -60.f, 3.f, juce::Colour(0xff00ff88), false);

    // GR peak-hold tick
    {
        auto b = bar(grArea);
        float t = juce::jlimit(0.f, 1.f, grPeakHold / 24.f);
        float y = b.getY() + b.getHeight() * t;
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(b.getX(), y - 1.f, b.getWidth(), 2.f);
    }

    // Scale ticks + micro labels inside each column
    auto drawScale = [&](juce::Rectangle<float> col, float minDB, float maxDB, bool inverted,
                         std::initializer_list<float> steps)
    {
        auto b = bar(col);
        g.setFont(7.5f);
        for (float s : steps)
        {
            float t = juce::jlimit(0.f, 1.f, (s - minDB) / (maxDB - minDB));
            float y = inverted ? b.getY() + b.getHeight() * t
                               : b.getBottom() - b.getHeight() * t;
            g.setColour(juce::Colours::white.withAlpha(0.28f));
            g.fillRect(b.getX(), y - 0.5f, 5.f, 1.f);
            g.setColour(juce::Colour(0xff8a93a6).withAlpha(0.9f));
            juce::String txt = (std::abs(s) < 0.5f) ? "0" : juce::String((int)s);
            g.drawText(txt, (int)(b.getX() + 6), (int)(y - 6), 26, 10, juce::Justification::left);
        }
    };
    drawScale(grArea, 0.f, 24.f, true, { 0, 6, 12, 18, 24 });
    drawScale(inArea, -60.f, 3.f, false, { 0, -12, -24, -48 });
    drawScale(outArea, -60.f, 3.f, false, { 0, -12, -24, -48 });

    // Numeric readouts
    g.setFont(11.f);
    g.setColour(juce::Colours::white);
    float y0 = bounds.getBottom() - 34;
    g.drawText(juce::String(gr, 1), (int)grArea.getX(), (int)y0, (int)grArea.getWidth(), 14, juce::Justification::centred);
    g.drawText(juce::String((int)inDB), (int)inArea.getX(), (int)y0, (int)inArea.getWidth(), 14, juce::Justification::centred);
    g.drawText(juce::String((int)outDB), (int)outArea.getX(), (int)y0, (int)outArea.getWidth(), 14, juce::Justification::centred);
    g.setFont(9.f);
    g.setColour(juce::Colour(0xff6a6a80));
    g.drawText("dB", (int)grArea.getX(), (int)(y0 + 13), (int)grArea.getWidth(), 12, juce::Justification::centred);
    g.drawText("dBFS", (int)inArea.getX(), (int)(y0 + 13), (int)inArea.getWidth(), 12, juce::Justification::centred);
    g.drawText("dBFS", (int)outArea.getX(), (int)(y0 + 13), (int)outArea.getWidth(), 12, juce::Justification::centred);

    g.setColour(juce::Colour(0xff1e1e2a));
    g.drawRect(bounds, 1.5f);
}
