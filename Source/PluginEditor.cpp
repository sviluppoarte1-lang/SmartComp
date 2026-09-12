#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <array>

SmartCompAudioProcessorEditor::KnobCell::KnobCell(const juce::String& name, juce::Colour accent)
{
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(11.f).withStyle(juce::Font::bold));
    label.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(label);
    knob.setColour(juce::Slider::thumbColourId, accent);
    addAndMakeVisible(knob);
}

void SmartCompAudioProcessorEditor::KnobCell::resized()
{
    auto b = getLocalBounds();
    label.setBounds(b.removeFromTop(16));
    knob.setBounds(b);
}

// Demo-expired overlay (buy link only - no key field in the public build)
struct SmartCompAudioProcessorEditor::ExpiredOverlay : public juce::Component
{
    juce::Label msg;
    juce::TextButton buy { "BUY FULL VERSION - 19.99 EUR" };

    ExpiredOverlay()
    {
        msg.setText("Demo expired after 45 minutes of use - audio is muted.\nBuy the full version for unlimited use.",
                    juce::dontSendNotification);
        msg.setJustificationType(juce::Justification::centred);
        msg.setColour(juce::Label::textColourId, juce::Colours::white);
        msg.setFont(juce::Font(15.f).withStyle(juce::Font::bold));
        addAndMakeVisible(msg);

        buy.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2962ff));
        buy.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(buy);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xe80a0a0f));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop(juce::jmax(20, getHeight() / 3));
        msg.setBounds(b.removeFromTop(80));
        b.removeFromTop(10);
        auto btnRow = b.removeFromTop(40);
        buy.setBounds(btnRow.withSizeKeepingCentre(300, 36));
    }
};

// Public shop link (payment address, not a secret)
static constexpr const char* kShopUrl = "https://www.paypal.com/paypalme/fearescape/19.99";

static const char* kKnobNames[13] = {
    "Threshold", "Ratio", "Attack", "Release", "Knee", "Makeup",
    "Mix", "SC HPF", "Detector", "Ceiling", "Drive", "Input", "Output"
};
static const char* kKnobIds[13] = {
    "threshold", "ratio", "attack", "release", "knee", "makeup",
    "mix", "scHPF", "detector", "ceiling", "drive", "inputGain", "outputGain"
};

SmartCompAudioProcessorEditor::SmartCompAudioProcessorEditor(SmartCompAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    // Do NOT call setSize here (Bitwig discovery crash guard) - at the end.

    curve = std::make_unique<CompCurveComponent>(processor.getComp());
    addAndMakeVisible(*curve);
    meters = std::make_unique<CompMeterComponent>(processor.getComp());
    addAndMakeVisible(*meters);

    titleLabel.setText("SmartComp  |  Leveling Compressor - Limiter  |  Fear Escape", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.f).withStyle(juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Preset browser
    isInitializing = true;
    categoryLabel.setText("Category:", juce::dontSendNotification); categoryLabel.setFont(11.f);
    presetLabel.setText("Preset:", juce::dontSendNotification); presetLabel.setFont(11.f);
    profileLabel.setText("Profile:", juce::dontSendNotification); profileLabel.setFont(11.f);
    addAndMakeVisible(categoryLabel); addAndMakeVisible(categoryBox);
    addAndMakeVisible(presetLabel); addAndMakeVisible(presetBox);
    addAndMakeVisible(profileLabel); addAndMakeVisible(profileBox);
    categoryBox.addItemList(processor.getPresetManager().getCategories(), 1);
    {
        auto lastCat = processor.getLastPresetCategory();
        if (lastCat.isNotEmpty() && processor.getPresetManager().getCategories().contains(lastCat))
            categoryBox.setSelectedId(processor.getPresetManager().getCategories().indexOf(lastCat) + 1, juce::dontSendNotification);
        else
            categoryBox.setSelectedId(1, juce::dontSendNotification);
    }
    updatePresetBox();
    {
        auto lastName = processor.getLastPresetName();
        if (lastName.isNotEmpty() && processor.getPresetManager().findPresetByName(lastName) >= 0)
        {
            presetBox.setText(lastName, juce::dontSendNotification);
            statusLabel.setText("Preset: " + lastName, juce::dontSendNotification);
        }
    }
    categoryBox.onChange = [this]{ if (!isInitializing) updatePresetBox(); };
    presetBox.onChange = [this]{ if (!isInitializing) applyPresetFromBox(); };

    profileBox.addItemList(AutoLeveler::classNames(), 1);
    profileBox.setSelectedId(1, juce::dontSendNotification);
    profileBox.setTooltip("Helps the auto-leveler: pick the source, or Auto-Detect");

    circuitLabel.setText("Circuit:", juce::dontSendNotification); circuitLabel.setFont(11.f);
    addAndMakeVisible(circuitLabel);
    circuitBox.addItemList(CircuitModelInfo::names(), 1);
    circuitBox.setSelectedId(1, juce::dontSendNotification);
    circuitBox.setTooltip("Historic circuit model: detector behavior + saturation character");
    addAndMakeVisible(circuitBox);

    // LEARN + toggles
    learnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff3b30));
    learnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    learnButton.setTooltip("Listen 4 seconds, detect the source, set the compressor");
    learnButton.onClick = [this]{
        if (learning) return;
        processor.requestLearn();
        learning = true;
        learnButton.setButtonText("LISTEN...");
    };
    addAndMakeVisible(learnButton);

    // Song-map: learn level/crest per bar, then ride threshold+ratio along the song
    songLearnButton.setClickingTogglesState(true);
    songLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    songLearnButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb71c1c));
    songLearnButton.setTooltip("SONG LEARN: play the whole song - SmartComp memorizza livello e crest battuta per battuta");
    addAndMakeVisible(songLearnButton);
    songFollowButton.setClickingTogglesState(true);
    songFollowButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    songFollowButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00c853));
    songFollowButton.setTooltip("FOLLOW: threshold e ratio seguono la mappa del brano (glide 400ms)");
    addAndMakeVisible(songFollowButton);
    songClearButton.setTooltip("Cancella la mappa del brano");
    songClearButton.onClick = [this]{
        processor.clearSongMap();
        statusLabel.setText("Song map cleared", juce::dontSendNotification);
    };
    addAndMakeVisible(songClearButton);

    autoLevelButton.setTooltip("Continuous auto level riding");
    limiterButton.setTooltip("Brickwall limiter mode");
    bypassButton.setTooltip("Bypass");
    autoLevelButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00ff88));
    limiterButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffa500));
    bypassButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffcc00));
    autoRelButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00ff88));
    autoMkButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00ff88));
    addAndMakeVisible(autoLevelButton);
    addAndMakeVisible(limiterButton);
    addAndMakeVisible(bypassButton);

    // Knobs
    const juce::Colour accents[13] = {
        juce::Colour(0xffff3b30), juce::Colour(0xffffa500), juce::Colour(0xffffcc00), juce::Colour(0xff9a7bff),
        juce::Colour(0xff00d4ff), juce::Colour(0xff00ff88), juce::Colour(0xff00d4ff), juce::Colour(0xffc07bff),
        juce::Colour(0xff7bff7b), juce::Colour(0xffffcc00), juce::Colour(0xffff6b4a), juce::Colour(0xff8a93a6), juce::Colour(0xff8a93a6)
    };
    for (int i = 0; i < 13; ++i)
    {
        auto* cell = new KnobCell(kKnobNames[i], accents[i]);
        cell->knob.setLookAndFeel(&modernLF);
        cell->knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        cell->knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
        knobCells.add(cell);
        addAndMakeVisible(cell);
    }
    // Knob ranges + suffixes
    auto& thr = knobCells[0]->knob;  thr.setRange(-60, 0, 0.1); thr.setTextValueSuffix(" dB");
    auto& rat = knobCells[1]->knob;  rat.setRange(1, 20, 0.1); rat.setTextValueSuffix(":1");
    auto& atk = knobCells[2]->knob;  atk.setRange(0.02, 300, 0.01); atk.setSkewFactorFromMidPoint(5); atk.setTextValueSuffix(" ms");
    auto& rel = knobCells[3]->knob;  rel.setRange(20, 3000, 1); rel.setSkewFactorFromMidPoint(200); rel.setTextValueSuffix(" ms");
    auto& kne = knobCells[4]->knob;  kne.setRange(0, 12, 0.1); kne.setTextValueSuffix(" dB");
    auto& mk  = knobCells[5]->knob;  mk.setRange(0, 24, 0.1); mk.setTextValueSuffix(" dB");
    auto& mix = knobCells[6]->knob;  mix.setRange(0, 100, 1); mix.setTextValueSuffix(" %");
    auto& scf = knobCells[7]->knob;  scf.setRange(20, 500, 1); scf.setSkewFactorFromMidPoint(100); scf.setTextValueSuffix(" Hz");
    auto& det = knobCells[8]->knob;  det.setRange(0, 1, 0.01);
    auto& cei = knobCells[9]->knob;  cei.setRange(-12, 0, 0.1); cei.setTextValueSuffix(" dB");
    auto& drv = knobCells[10]->knob; drv.setRange(0, 24, 0.1); drv.setTextValueSuffix(" dB");
    drv.setTooltip("Saturation drive on top of the circuit color");
    auto& inG = knobCells[11]->knob; inG.setRange(-24, 12, 0.1); inG.setTextValueSuffix(" dB");
    auto& ouG = knobCells[12]->knob; ouG.setRange(-24, 12, 0.1); ouG.setTextValueSuffix(" dB");

    autoRelButton.setTooltip("Program-dependent auto release (SSL-style glue)");
    autoMkButton.setTooltip("Match output loudness to input automatically");
    autoRelButton.setLookAndFeel(&modernLF);
    autoMkButton.setLookAndFeel(&modernLF);
    addAndMakeVisible(autoRelButton);
    addAndMakeVisible(autoMkButton);

    // Section titles: real labels in their own strip, left aligned per panel
    for (auto* l : { &secDynLabel, &secLvlLabel, &secOutLabel })
    {
        l->setFont(juce::Font(10.f, juce::Font::bold));
        l->setColour(juce::Label::textColourId, juce::Colour(0xff6a7186));
        l->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
    }
    secDynLabel.setText("DYNAMICS", juce::dontSendNotification);
    secLvlLabel.setText("LEVEL", juce::dontSendNotification);
    secOutLabel.setText("OUTPUT", juce::dontSendNotification);

    // Detection readout
    detLabel.setFont(juce::Font(13.f).withStyle(juce::Font::bold));
    detLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d4ff));
    detLabel.setJustificationType(juce::Justification::centred);
    detLabel.setText("Listening...", juce::dontSendNotification);
    addAndMakeVisible(detLabel);
    featLabel.setFont(juce::Font(10.f));
    featLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6a6a80));
    featLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(featLabel);

    statusLabel.setFont(juce::Font(11.f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setText("Pick a preset or press LEARN while audio plays.", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    // Demo bar: countdown + buy link, no key field in the public build
    demoLabel.setFont(juce::Font(11.f).withStyle(juce::Font::bold));
    demoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffcc00));
    addAndMakeVisible(demoLabel);
    buyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2962ff));
    buyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    buyButton.setTooltip("Buy the full version - opens PayPal in your browser");
    buyButton.onClick = []{ juce::URL(kShopUrl).launchInDefaultBrowser(); };
    addAndMakeVisible(buyButton);

    expiredOverlay = std::make_unique<ExpiredOverlay>();
    expiredOverlay->buy.setLookAndFeel(&modernLF);
    expiredOverlay->buy.onClick = []{ juce::URL(kShopUrl).launchInDefaultBrowser(); };
    addChildComponent(*expiredOverlay);

    // Modern styling for the rest
    for (auto* b : { &categoryBox, &presetBox, &profileBox, &circuitBox })
        b->setLookAndFeel(&modernLF);
    for (juce::Button* b : std::array<juce::Button*, 2> { &learnButton, &bypassButton })
        b->setLookAndFeel(&modernLF);
    buyButton.setLookAndFeel(&modernLF);
    songLearnButton.setLookAndFeel(&modernLF);
    songFollowButton.setLookAndFeel(&modernLF);
    songClearButton.setLookAndFeel(&modernLF);
    autoLevelButton.setLookAndFeel(&modernLF);
    limiterButton.setLookAndFeel(&modernLF);
    bypassButton.setLookAndFeel(&modernLF);

    // Attachments
    const char* ids[13] = { "threshold","ratio","attack","release","knee","makeup","mix","scHPF","detector","ceiling","drive","inputGain","outputGain" };
    for (int i = 0; i < 13; ++i)
        knobAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
            processor.apvts, ids[i], knobCells[i]->knob));
    songLearnAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "songLearn", songLearnButton);
    songFollowAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "songFollow", songFollowButton);
    bypassAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "bypass", bypassButton);
    limiterAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "limiter", limiterButton);
    autoLevelAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "autoLevel", autoLevelButton);
    autoRelAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "autoRelease", autoRelButton);
    autoMkAttach = new juce::AudioProcessorValueTreeState::ButtonAttachment(processor.apvts, "autoMakeup", autoMkButton);
    profileAttach = new juce::AudioProcessorValueTreeState::ComboBoxAttachment(processor.apvts, "profile", profileBox);
    circuitAttach = new juce::AudioProcessorValueTreeState::ComboBoxAttachment(processor.apvts, "circuit", circuitBox);

    isInitializing = false;
    setResizable(true, true);
    setResizeLimits(1100, 700, 1920, 1200);
    setSize(1280, 840);
    startTimerHz(15);
}

SmartCompAudioProcessorEditor::~SmartCompAudioProcessorEditor()
{
    stopTimer();
    for (auto* c : knobCells) c->knob.setLookAndFeel(nullptr);
    for (auto* b : { &categoryBox, &presetBox, &profileBox, &circuitBox }) b->setLookAndFeel(nullptr);
    for (juce::Button* b : std::array<juce::Button*, 2> { &learnButton, &bypassButton }) b->setLookAndFeel(nullptr);
    buyButton.setLookAndFeel(nullptr);
    if (expiredOverlay) expiredOverlay->buy.setLookAndFeel(nullptr);
    songLearnButton.setLookAndFeel(nullptr);
    songFollowButton.setLookAndFeel(nullptr);
    songClearButton.setLookAndFeel(nullptr);
    autoLevelButton.setLookAndFeel(nullptr);
    limiterButton.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    autoRelButton.setLookAndFeel(nullptr);
    autoMkButton.setLookAndFeel(nullptr);
    delete songLearnAttach; delete songFollowAttach;
    delete bypassAttach; delete limiterAttach; delete autoLevelAttach;
    delete autoRelAttach; delete autoMkAttach; delete profileAttach; delete circuitAttach;
}

void SmartCompAudioProcessorEditor::setParam(const juce::String& id, float value)
{
    if (auto* par = processor.apvts.getParameter(id))
        par->setValueNotifyingHost(par->convertTo0to1(value));
}

void SmartCompAudioProcessorEditor::updatePresetBox()
{
    auto cat = categoryBox.getText();
    presetBox.setSelectedId(0, juce::dontSendNotification);
    presetBox.clear(juce::dontSendNotification);
    auto list = processor.getPresetManager().getPresetsForCategory(cat);
    int id = 1;
    for (auto& p : list) presetBox.addItem(p.name, id++);
    if (presetBox.getNumItems() > 0) presetBox.setSelectedId(0, juce::dontSendNotification);
    presetBox.setText("Choose preset...", juce::dontSendNotification);
    auto lastName = processor.getLastPresetName();
    if (lastName.isNotEmpty() && processor.getPresetManager().findPresetByName(lastName) >= 0)
    {
        auto& last = processor.getPresetManager().getPreset(processor.getPresetManager().findPresetByName(lastName));
        if (last.category == cat) presetBox.setText(lastName, juce::dontSendNotification);
    }
}

void SmartCompAudioProcessorEditor::applyPresetFromBox()
{
    if (isInitializing) return;
    auto name = presetBox.getText();
    if (name == "Choose preset..." || name.isEmpty()) return;
    int idx = processor.getPresetManager().findPresetByName(name);
    if (idx < 0) return;
    auto& p = processor.getPresetManager().getPreset(idx);
    setParam("threshold", (float)p.params.thresholdDB);
    setParam("ratio", (float)p.params.ratio);
    setParam("attack", (float)p.params.attackMs);
    setParam("release", (float)p.params.releaseMs);
    setParam("knee", (float)p.params.kneeDB);
    setParam("makeup", (float)p.params.makeupDB);
    setParam("mix", (float)(p.params.mix * 100.0));
    setParam("scHPF", (float)p.params.scHPF);
    setParam("detector", (float)p.params.detectorMix);
    setParam("ceiling", (float)p.params.ceilingDB);
    if (auto* par = processor.apvts.getParameter("limiter")) par->setValueNotifyingHost(par->convertTo0to1(p.params.limiter ? 1.f : 0.f));
    if (auto* par = processor.apvts.getParameter("autoRelease")) par->setValueNotifyingHost(par->convertTo0to1(p.autoRelease ? 1.f : 0.f));
    if (auto* par = processor.apvts.getParameter("autoMakeup")) par->setValueNotifyingHost(par->convertTo0to1(p.autoMakeup ? 1.f : 0.f));
    if (auto* par = processor.apvts.getParameter("autoLevel")) par->setValueNotifyingHost(par->convertTo0to1(p.autoLevel ? 1.f : 0.f));
    if (auto* par = processor.apvts.getParameter("profile")) par->setValueNotifyingHost(par->convertTo0to1((float)p.profile));
    if (auto* par = processor.apvts.getParameter("circuit")) par->setValueNotifyingHost(par->convertTo0to1((float)p.model));
    setParam("drive", (float)p.drive);
    statusLabel.setText("Preset: " + p.name + " - " + p.description, juce::dontSendNotification);
    processor.setLastPreset(p.name, p.category);
}

void SmartCompAudioProcessorEditor::applyLearnResult(const SmartCompAudioProcessor::LearnResult& r)
{
    setParam("threshold", (float)r.thresholdDB);
    setParam("ratio", (float)r.autoParams.ratio);
    setParam("attack", (float)r.autoParams.attackMs);
    setParam("release", (float)r.autoParams.releaseMs);
    setParam("knee", (float)r.autoParams.kneeDB);
    setParam("scHPF", (float)r.autoParams.scHPF);
    setParam("makeup", 0.f);
    if (auto* par = processor.apvts.getParameter("autoRelease")) par->setValueNotifyingHost(par->convertTo0to1(r.autoParams.autoRelease ? 1.f : 0.f));
    if (auto* par = processor.apvts.getParameter("autoMakeup")) par->setValueNotifyingHost(1.f);
    juce::String msg = "Learned: ";
    msg += AutoLeveler::className(r.cls);
    msg += " (" + juce::String((int)(r.confidence * 100)) + "%) - Thr ";
    msg += juce::String(r.thresholdDB, 1) + "dB, Ratio " + juce::String(r.autoParams.ratio, 1);
    msg += ":1, Atk " + juce::String(r.autoParams.attackMs, 1) + "ms, Rel " + juce::String((int)r.autoParams.releaseMs) + "ms";
    statusLabel.setText(msg, juce::dontSendNotification);
}

void SmartCompAudioProcessorEditor::drawSectionPanel(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty()) return;
    auto f = area.toFloat().reduced(-4, 2);
    g.setColour(juce::Colour(0xff11131c));
    g.fillRoundedRectangle(f, 8.0f);
    g.setColour(juce::Colour(0xff232838));
    g.drawRoundedRectangle(f, 8.0f, 1.0f);
}

void SmartCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0f));
    // Knob section panels sit behind titles + knob cells (titles are real
    // labels in their own strip, so no text can ever overlap)
    if (getWidth() > 50 && getHeight() > 50)
    {
        drawSectionPanel(g, panelDyn);
        drawSectionPanel(g, panelLvl);
        drawSectionPanel(g, panelOut);
    }
    auto top = getLocalBounds().removeFromTop(38).toFloat();
    juce::ColourGradient grad(juce::Colour(0xff1a1a2e), top.getCentreX(), top.getY(),
                              juce::Colour(0xff0f0f1a), top.getCentreX(), top.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(top);
    g.setColour(juce::Colour(0xff2a2a40));
    g.drawHorizontalLine((int)top.getBottom(), top.getX(), top.getRight());
}

void SmartCompAudioProcessorEditor::resized()
{
    if (knobCells.size() < 13) return;
    if (getWidth() < 50 || getHeight() < 50) return;
    auto b = getLocalBounds();
    titleLabel.setBounds(b.removeFromTop(38));

    auto controls = b.removeFromTop(52);
    controls.reduce(6, 4);
    auto left = controls;
    auto catArea = left.removeFromLeft(140);
    categoryLabel.setBounds(catArea.removeFromTop(14));
    categoryBox.setBounds(catArea.removeFromTop(26));
    left.removeFromLeft(6);
    auto presetArea = left.removeFromLeft(160);
    presetLabel.setBounds(presetArea.removeFromTop(14));
    presetBox.setBounds(presetArea.removeFromTop(26));
    left.removeFromLeft(6);
    auto profArea = left.removeFromLeft(140);
    profileLabel.setBounds(profArea.removeFromTop(14));
    profileBox.setBounds(profArea.removeFromTop(26));
    left.removeFromLeft(6);
    auto circArea = left.removeFromLeft(140);
    circuitLabel.setBounds(circArea.removeFromTop(14));
    circuitBox.setBounds(circArea.removeFromTop(26));
    left.removeFromLeft(10);
    learnButton.setBounds(left.removeFromLeft(110));
    left.removeFromLeft(6);
    autoLevelButton.setBounds(left.removeFromLeft(92).removeFromTop(30).withTrimmedTop(6));
    limiterButton.setBounds(left.removeFromLeft(92).removeFromTop(30).withTrimmedTop(6));
    bypassButton.setBounds(left.removeFromLeft(88).removeFromTop(30).withTrimmedTop(6));
    left.removeFromLeft(6);
    songLearnButton.setBounds(left.removeFromLeft(88).removeFromTop(30).withTrimmedTop(6));
    left.removeFromLeft(4);
    songFollowButton.setBounds(left.removeFromLeft(78).removeFromTop(30).withTrimmedTop(6));
    left.removeFromLeft(4);
    songClearButton.setBounds(left.removeFromLeft(52).removeFromTop(30).withTrimmedTop(6));

    // Bottom: demo bar, knobs, status, then everything else to curve+meters
    constexpr int demoH = 32;
    auto demoBar = b.removeFromBottom(demoH);
    demoBar.reduce(6, 3);
    demoLabel.setBounds(demoBar.removeFromLeft(330));
    demoBar.removeFromLeft(6);
    buyButton.setBounds(demoBar.removeFromRight(250));

    constexpr int knobH = 198;
    auto knobRow = b.removeFromBottom(knobH);
    knobRow.reduce(6, 2);
    // Section title strip: its own y-band above the knobs, so titles can
    // never overlap knob labels or value boxes
    auto titleStrip = knobRow.removeFromTop(16);
    {
        int tw = titleStrip.getWidth();
        secDynLabel.setBounds(titleStrip.removeFromLeft(tw * 5 / 13).reduced(6, 0));
        secLvlLabel.setBounds(titleStrip.removeFromLeft(tw * 4 / 13).reduced(6, 0));
        secOutLabel.setBounds(titleStrip.reduced(6, 0));
    }
    int cellW = knobRow.getWidth() / 13;
    juce::Rectangle<int> toggleAreas[13];
    for (int i = 0; i < 13; ++i)
    {
        auto cellArea = knobRow.removeFromLeft(cellW).reduced(2, 0);
        if (i == 3 || i == 5)
        {
            // Dedicated AUTO toggle space carved from the cell bottom:
            // the slider (with its value box) keeps the rest, zero overlap
            toggleAreas[i] = cellArea.removeFromBottom(20);
        }
        knobCells[i]->setBounds(cellArea);
    }
    autoRelButton.setBounds(toggleAreas[3].reduced(10, 2));
    autoMkButton.setBounds(toggleAreas[5].reduced(10, 2));
    // Section panels behind titles + knob groups
    if (knobCells.size() >= 13)
    {
        panelDyn = secDynLabel.getBounds().getUnion(knobCells[0]->getBounds().getUnion(knobCells[4]->getBounds()));
        panelLvl = secLvlLabel.getBounds().getUnion(knobCells[5]->getBounds().getUnion(knobCells[8]->getBounds()));
        panelOut = secOutLabel.getBounds().getUnion(knobCells[9]->getBounds().getUnion(knobCells[12]->getBounds()));
    }

    constexpr int statusH = 22;
    auto statusArea = b.removeFromBottom(statusH);
    statusLabel.setBounds(statusArea);

    // Middle: curve (left ~62%) + meters + detection readout
    b.reduce(6, 2);
    auto midRight = b.removeFromRight(300);
    curve->setBounds(b);
    auto detArea = midRight.removeFromTop(64);
    detLabel.setBounds(detArea.removeFromTop(26));
    featLabel.setBounds(detArea);
    meters->setBounds(midRight);

    auto overlayArea = getLocalBounds();
    overlayArea.removeFromBottom(demoH);
    expiredOverlay->setBounds(overlayArea);
}

void SmartCompAudioProcessorEditor::timerCallback()
{
    // LEARN progress
    if (learning)
    {
        float p = processor.learnProgress();
        learnButton.setButtonText("LISTEN " + juce::String((int)(p * 100)) + "%");
        if (!processor.isLearning())
        {
            learning = false;
            learnButton.setButtonText("LEARN");
            auto res = processor.takeLearnResult();
            if (res.ready && res.features.valid) applyLearnResult(res);
            else statusLabel.setText("Learn needs louder audio - play the track and try again.", juce::dontSendNotification);
        }
    }

    // Detection readout
    {
        int cls = processor.getDetectedClass();
        float conf = processor.getDetectedConfidence();
        auto f = processor.getLevelerFeatures();
        if (f.valid)
        {
            juce::String dc = "Hearing: ";
            dc += (cls >= 0 && cls < 9) ? AutoLeveler::className((AutoLeveler::SourceClass)cls) : "?";
            dc += " " + juce::String((int)(conf * 100)) + "%";
            if (detLabel.getText() != dc) detLabel.setText(dc, juce::dontSendNotification);
            juce::String ft = "Crest " + juce::String(f.crestDB, 1) + "dB | Dyn " + juce::String(f.dynRangeDB, 1);
            ft += "dB | Thr " + juce::String(processor.getEffectiveThreshold(), 1) + "dB | GRavg " + juce::String(processor.getAvgGR(), 1) + "dB";
            ft += "  |  " + processor.getSongDynamics().getStatusText();
            if (featLabel.getText() != ft) featLabel.setText(ft, juce::dontSendNotification);
        }
        else if (detLabel.getText() != "Listening...")
            detLabel.setText("Listening...", juce::dontSendNotification);
    }

    // Demo countdown + expiry overlay
    double rem = processor.getDemoSecondsRemaining();
    int m = (int)rem / 60, s = (int)rem % 60;
    juce::String t = "Demo: " + juce::String(m) + ":" + (s < 10 ? "0" : "") + juce::String(s) + " remaining";
    if (demoLabel.getText() != t) demoLabel.setText(t, juce::dontSendNotification);
    demoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffcc00));
    bool expired = processor.isDemoExpired();
    if (expiredOverlay)
    {
        if (expired && !expiredOverlay->isVisible()) expiredOverlay->setVisible(true);
        else if (!expired && expiredOverlay->isVisible()) expiredOverlay->setVisible(false);
    }
    repaint();
}

juce::AudioProcessorEditor* SmartCompAudioProcessor::createEditor()
{
    return new SmartCompAudioProcessorEditor(*this);
}
