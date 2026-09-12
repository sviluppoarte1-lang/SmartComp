#include "PluginProcessor.h"
#include "PluginEditor.h"

SmartCompAudioProcessor::SmartCompAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Public build: always unlocked, no license handling.
}

SmartCompAudioProcessor::~SmartCompAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SmartCompAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("threshold", "Threshold",
        juce::NormalisableRange<float>(-60.f, 0.f, 0.1f), -18.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ratio", "Ratio",
        juce::NormalisableRange<float>(1.f, 20.f, 0.1f), 2.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("attack", "Attack",
        juce::NormalisableRange<float>(0.02f, 300.f, 0.01f, 0.35f), 10.f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("circuit", "Circuit Model",
        CircuitModelInfo::names(), 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("drive", "Drive",
        juce::NormalisableRange<float>(0.f, 24.f, 0.1f), 0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("release", "Release",
        juce::NormalisableRange<float>(20.f, 3000.f, 1.f, 0.35f), 200.f));
    layout.add(std::make_unique<juce::AudioParameterBool>("autoRelease", "Auto Release", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("knee", "Knee",
        juce::NormalisableRange<float>(0.f, 12.f, 0.1f), 6.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("makeup", "Makeup",
        juce::NormalisableRange<float>(0.f, 24.f, 0.1f), 0.f));
    layout.add(std::make_unique<juce::AudioParameterBool>("autoMakeup", "Auto Makeup", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mix", "Mix",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f), 100.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("scHPF", "Sidechain HPF",
        juce::NormalisableRange<float>(20.f, 500.f, 1.f, 0.4f), 30.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("detector", "Detector Peak/RMS",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain",
        juce::NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain",
        juce::NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f));
    layout.add(std::make_unique<juce::AudioParameterBool>("limiter", "Limiter", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ceiling", "Ceiling",
        juce::NormalisableRange<float>(-12.f, 0.f, 0.1f), -0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("profile", "Profile",
        AutoLeveler::classNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterBool>("autoLevel", "Auto Level", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    // Song-map: learn level/crest per bar, then ride threshold+ratio along the song
    layout.add(std::make_unique<juce::AudioParameterBool>("songLearn", "Song Learn", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("songFollow", "Song Follow", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("songGlide", "Song Glide",
        juce::NormalisableRange<float>(0.f, 2000.f, 1.f, 0.4f), 400.f));
    return layout;
}

static float getFloat(juce::AudioProcessorValueTreeState& apvts, const char* id, float fallback = 0.f)
{
    if (auto* p = apvts.getRawParameterValue(id)) return p->load();
    return fallback;
}
static bool getBool(juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    if (auto* p = apvts.getRawParameterValue(id)) return p->load() > 0.5f;
    return false;
}

void SmartCompAudioProcessor::syncCompParams(double /*sr*/, int numSamples)
{
    CompParams p;
    p.thresholdDB = getFloat(apvts, "threshold", -18.f);
    p.ratio       = getFloat(apvts, "ratio", 2.f);
    p.attackMs    = getFloat(apvts, "attack", 10.f);
    p.releaseMs   = getFloat(apvts, "release", 200.f);
    p.autoRelease = getBool(apvts, "autoRelease");
    p.kneeDB      = getFloat(apvts, "knee", 6.f);
    p.makeupDB    = getFloat(apvts, "makeup", 0.f);
    p.autoMakeup  = getBool(apvts, "autoMakeup");
    p.mix         = getFloat(apvts, "mix", 100.f) / 100.f;
    p.scHPF       = getFloat(apvts, "scHPF", 30.f);
    p.detectorMix = getFloat(apvts, "detector", 0.5f);
    p.limiter     = getBool(apvts, "limiter");
    p.ceilingDB   = getFloat(apvts, "ceiling", -0.5f);
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("circuit")))
        p.circuitModel = juce::jlimit(0, (int)CircuitModelInfo::NumModels - 1, pc->getIndex());
    p.driveDB = getFloat(apvts, "drive", 0.f);
    // Learn listen safety: while capturing, cap the saturation drive so the
    // 4 s monitoring never distorts - not even on All-Buttons-style settings.
    // The cap lifts automatically the moment capture completes.
    if (leveler.learnProgress() > 0.0f && !leveler.learnReady())
        p.driveDB = juce::jmin(p.driveDB, 6.0);

    // Continuous AUTO rides the threshold (level riding); threshold knob is the fallback
    bool autoOn = getBool(apvts, "autoLevel");
    if (autoOn)
    {
        int profIdx = 0;
        if (auto* pp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("profile")))
            profIdx = pp->getIndex();
        AutoLeveler::SourceClass locked = (profIdx <= 0) ? AutoLeveler::SourceClass::Unknown
                                                         : (AutoLeveler::SourceClass)(profIdx - 1);
        double avgGR = 0;
        double thr = leveler.updateAuto(p.thresholdDB, numSamples, locked, avgGR);
        p.thresholdDB = thr;
        avgGRmeter.store((float)avgGR);
    }
    applySongFollow(p, numSamples);
    effectiveThr.store((float)p.thresholdDB);
    comp.setParams(p);
}

void SmartCompAudioProcessor::applySongFollow(CompParams& p, int numSamples)
{
    auto* pFollow = apvts.getRawParameterValue("songFollow");
    auto* pGlide = apvts.getRawParameterValue("songGlide");
    bool follow = pFollow && pFollow->load() > 0.5f;
    float glideMs = pGlide ? pGlide->load() : 400.0f;

    if (! follow || ! songDyn.hasData())
    {
        songThrDelta = 0.0f;
        songRatioDelta = 0.0f;
        return;
    }

    SongDynamics::BarEntry e;
    float avgRms = 0, avgCrest = 0;
    bool have = (lastPpqValid ? songDyn.getEntryForPpq(lastPpq, e) : songDyn.getEntryForBar(0, e))
             && songDyn.getAverages(avgRms, avgCrest);

    // Threshold rides the bar level (constant relative depth); ratio follows crest.
    // Senza target (pause / mappa silente) scivola a zero: mai delta congelati.
    float targetThr = 0.0f, targetRatio = 0.0f;
    if (have)
    {
        targetThr = juce::jlimit(-12.0f, 12.0f, e.rmsDB - avgRms);
        targetRatio = juce::jlimit(-2.0f, 2.0f, (e.crestDB - avgCrest) * 0.3f);
    }

    float alpha = 1.0f;
    double sr = comp.getSampleRate();
    if (glideMs > 1.0f && sr > 0 && numSamples > 0)
        alpha = 1.0f - std::exp(-2.2f * (float) numSamples / ((glideMs * 0.001f) * (float) sr));
    alpha = juce::jlimit(0.0f, 1.0f, alpha);

    songThrDelta += alpha * (targetThr - songThrDelta);
    songRatioDelta += alpha * (targetRatio - songRatioDelta);

    p.thresholdDB = juce::jlimit(-60.0, 0.0, p.thresholdDB + songThrDelta);
    p.ratio = juce::jlimit(1.0, 20.0, p.ratio + songRatioDelta);
}

void SmartCompAudioProcessor::clearSongMap()
{
    songDyn.reset();
    songThrDelta = 0.0f;
    songRatioDelta = 0.0f;
}

void SmartCompAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    if (sampleRate < 8000) sampleRate = 48000;
    comp.prepare(sampleRate);
    leveler.prepare(sampleRate);
    safety.prepare(sampleRate);
    songDyn.prepare(sampleRate);
    songThrDelta = 0.0f;
    songRatioDelta = 0.0f;
    syncCompParams(sampleRate, 512);
}

void SmartCompAudioProcessor::releaseResources() {}

bool SmartCompAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainIn = layouts.getChannelSet(true, 0);
    auto mainOut = layouts.getChannelSet(false, 0);
    if (mainIn.isDisabled() && mainOut.isDisabled()) return false;
    if (mainIn.size() > 2 || mainOut.size() > 2) return false;
    return true;
}

void SmartCompAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    double sr = getSampleRate();
    if (sr < 8000) sr = 48000;

    // Demo: 45 minutes of audio per session, then mute. Timer only, no keys.
    demoSecondsUsed.store(demoSecondsUsed.load() + (double)numSamples / sr);
    if (demoSecondsUsed.load() >= kDemoLimitSeconds)
    {
        buffer.clear();
        return;
    }

    float inDB = getFloat(apvts, "inputGain", 0.f);
    float outDB = getFloat(apvts, "outputGain", 0.f);
    bool isBypass = getBool(apvts, "bypass");
    float inLin = juce::Decibels::decibelsToGain(inDB);
    float outLin = juce::Decibels::decibelsToGain(outDB);
    if (!std::isfinite(inLin)) inLin = 1.0f;
    if (!std::isfinite(outLin)) outLin = 1.0f;
    buffer.applyGain(inLin);

    // Feed the leveler ear (pre-compression signal)
    try {
        if (buffer.getNumChannels() > 0)
            leveler.pushBlock(buffer.getReadPointer(0),
                              buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr,
                              numSamples);
    } catch (...) {}

    // Song-map learn: transport + per-bar level/crest capture
    try {
        auto* pLearn = apvts.getRawParameterValue("songLearn");
        songDyn.setLearning(pLearn && pLearn->load() > 0.5f);

        double ppq = -1.0, bpm = 0.0;
        bool playing = false, ppqValid = false;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto o = pos->getPpqPosition()) { ppq = *o; ppqValid = true; }
                if (auto o = pos->getBpm()) bpm = *o;
                playing = pos->getIsPlaying();
                if (! ppqValid)
                {
                    auto t = pos->getTimeInSeconds();
                    if (t && bpm > 0) { ppq = (*t) * bpm / 60.0; ppqValid = true; }
                }
            }
        }
        lastPpq = ppq;
        lastPpqValid = ppqValid;

        // Falling edge del transport: consolida l'ultima battuta (altrimenti si perde)
        if (songWasPlaying && ! playing)
            songDyn.flush();
        songWasPlaying = playing;

        if (songDyn.isLearning() && buffer.getNumChannels() > 0 && numSamples > 0)
            songDyn.pushAudioBlock(buffer.getReadPointer(0),
                                   buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr,
                                   numSamples, ppq, ppqValid, playing, bpm, sr);
    } catch (...) {}

    syncCompParams(sr, numSamples);

    if (!isBypass)
    {
        try { comp.processBlock(buffer); } catch (...) { buffer.clear(); }
    }
    leveler.noteGainReduction(comp.getGainReductionDB());

    buffer.applyGain(outLin);
    // Output safety limiter: digital clipping is impossible by construction,
    // in every profile and circuit - then sanitize + absolute clamp.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = safety.process(d[i]);
            if (!std::isfinite(s)) s = 0.0f;
            d[i] = juce::jlimit(-1.2f, 1.2f, s);
        }
    }

    // Live detection readout (~4x/sec)
    if (++detectCounter >= 128)
    {
        detectCounter = 0;
        auto f = leveler.getFeatures();
        int profIdx = 0;
        if (auto* pp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("profile")))
            profIdx = pp->getIndex();
        AutoLeveler::SourceClass prior = (profIdx <= 0) ? AutoLeveler::SourceClass::Unknown
                                                        : (AutoLeveler::SourceClass)(profIdx - 1);
        auto r = leveler.classify(f, prior);
        detectedClass.store((int)r.cls);
        detectedConf.store(r.confidence);
    }
}

void SmartCompAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuf(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i) floatBuf.setSample(ch, i, (float)buffer.getSample(ch, i));
    processBlock(floatBuf, midi);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i) buffer.setSample(ch, i, (double)floatBuf.getSample(ch, i));
}

// ---- LEARN ----
void SmartCompAudioProcessor::requestLearn() { leveler.beginLearn(); }
bool SmartCompAudioProcessor::isLearning() const { return !leveler.learnReady(); }
float SmartCompAudioProcessor::learnProgress() const { return leveler.learnProgress(); }

SmartCompAudioProcessor::LearnResult SmartCompAudioProcessor::takeLearnResult()
{
    LearnResult r;
    if (!leveler.learnReady()) return r;
    auto f = leveler.getFeatures();
    int profIdx = 0;
    if (auto* pp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("profile")))
        profIdx = pp->getIndex();
    AutoLeveler::SourceClass prior = (profIdx <= 0) ? AutoLeveler::SourceClass::Unknown
                                                    : (AutoLeveler::SourceClass)(profIdx - 1);
    auto cr = leveler.classify(f, prior);
    // If user locked a profile, the profile wins for mapping (detection still shown)
    AutoLeveler::SourceClass mapCls = (prior != AutoLeveler::SourceClass::Unknown) ? prior : cr.cls;
    if (mapCls == AutoLeveler::SourceClass::Unknown) mapCls = AutoLeveler::SourceClass::Mix;
    r.ready = true;
    r.cls = cr.cls;
    r.confidence = cr.confidence;
    r.features = f;
    r.autoParams = AutoLeveler::paramsForClass(mapCls);
    r.thresholdDB = juce::jlimit(-60.0, 0.0, f.rmsDB + r.autoParams.thrOffsetDB);
    return r;
}

// ---- Preset persistence ----
void SmartCompAudioProcessor::setLastPreset(const juce::String& name, const juce::String& category)
{
    apvts.state.setProperty("lastPresetName", name, nullptr);
    apvts.state.setProperty("lastPresetCategory", category, nullptr);
}
juce::String SmartCompAudioProcessor::getLastPresetName() const
{
    return apvts.state.getProperty("lastPresetName", "").toString();
}
juce::String SmartCompAudioProcessor::getLastPresetCategory() const
{
    return apvts.state.getProperty("lastPresetCategory", "").toString();
}

// ---- Demo (timer only, no license keys in the public build) ----
bool SmartCompAudioProcessor::isDemoExpired() const
{
    return demoSecondsUsed.load() >= kDemoLimitSeconds;
}
double SmartCompAudioProcessor::getDemoSecondsRemaining() const
{
    return juce::jlimit(0.0, kDemoLimitSeconds, kDemoLimitSeconds - demoSecondsUsed.load());
}

// createEditor() is defined in PluginEditor.cpp (keeps the crash-guard pattern)

void SmartCompAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild(songDyn.toValueTree(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SmartCompAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml) apvts.replaceState(juce::ValueTree::fromXml(*xml));
    auto songChild = apvts.state.getChildWithName("SongDyn");
    if (songChild.isValid())
        songDyn.restoreFromValueTree(songChild);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SmartCompAudioProcessor(); }
