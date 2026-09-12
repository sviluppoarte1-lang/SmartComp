// Song-follow smoke for SmartComp: map -> FOLLOW rides threshold,
// state save/load preserves the map.
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { ++fails; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
    else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setParam(SmartCompAudioProcessor& p, const char* id, float v)
{
    if (auto* par = p.apvts.getParameter(id))
        par->setValueNotifyingHost(par->convertTo0to1(v));
}

int main()
{
    juce::MidiBuffer midi;

    SongDynamics sd;
    sd.prepare(48000.0, 4);
    sd.setLearning(true);
    std::vector<float> blk(512);
    for (int b = 0; b < 200; ++b) // quiet bar
    {
        for (int i = 0; i < 512; ++i)
            blk[(size_t) i] = 0.15f * std::sin(2 * 3.14159265f * 220 * (b * 512 + i) / 48000.0f);
        sd.pushAudioBlock(blk.data(), nullptr, 512, b * 0.0213, true, true, 120.0, 48000.0);
    }
    for (int b = 0; b < 200; ++b) // loud bar
    {
        for (int i = 0; i < 512; ++i)
            blk[(size_t) i] = 0.5f * std::sin(2 * 3.14159265f * 220 * (b * 512 + i) / 48000.0f);
        sd.pushAudioBlock(blk.data(), nullptr, 512, 4.0 + b * 0.0213, true, true, 120.0, 48000.0);
    }
    sd.setLearning(false);
    CHECK(sd.getNumBars() >= 2, "offline map has %d bars", sd.getNumBars());

    SmartCompAudioProcessor proc;
    proc.prepareToPlay(48000, 512);
    proc.getSongDynamics().restoreFromValueTree(sd.toValueTree());
    setParam(proc, "threshold", -18.0f);
    setParam(proc, "songFollow", 1.0f);
    setParam(proc, "songGlide", 50.0f);

    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buf.setSample(ch, i, 0.3f * std::sin(2 * 3.14159265f * 440 * i / 48000.0f));
    for (int k = 0; k < 20; ++k) proc.processBlock(buf, midi);

    bool finite = true;
    for (int ch = 0; ch < 2 && finite; ++ch)
        for (int i = 0; i < 512; ++i)
            if (! std::isfinite(buf.getSample(ch, i))) finite = false;
    CHECK(finite, "follow output finite");

    // Bar 0 is quieter than average -> threshold should ride DOWN from -18
    float eff = proc.getEffectiveThreshold();
    printf("effective threshold %.2fdB (user -18)\n", eff);
    CHECK(eff < -18.0f, "threshold rides quiet bar down (%.2f)", eff);

    juce::MemoryBlock mb;
    proc.getStateInformation(mb);
    SmartCompAudioProcessor proc2;
    proc2.prepareToPlay(48000, 512);
    proc2.setStateInformation(mb.getData(), (int) mb.getSize());
    CHECK(proc2.getSongDynamics().getNumBars() == sd.getNumBars(),
          "map survives preset save/load (%d bars)", proc2.getSongDynamics().getNumBars());

    printf(fails == 0 ? "SONGFOLLOW ALL OK\n" : "SONGFOLLOW %d FAILURES\n", fails);
    return fails;
}
