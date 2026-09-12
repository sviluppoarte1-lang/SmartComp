// SmartComp learn anti-clip test (REAL processor, like a DAW would drive it).
// Scenario: worst case - All-Buttons-style settings (FET, drive 12, thr -24)
// + HOT broadband input. During the 4 s LEARN and after, output must never
// clip (peak <= -0.3 dBFS) and stay finite. Then takeLearnResult must be
// ready with a sane threshold.
// Compile: link against build/SmartComp_artefacts/Release/libSmartComp_SharedCode.a
// with the plugin's flags (see README tech section), plus juce modules from JUCE.
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
static void setChoice(SmartCompAudioProcessor& p, const char* id, int idx)
{
    if (auto* par = p.apvts.getParameter(id))
        par->setValueNotifyingHost(par->convertTo0to1((float)idx));
}

int main()
{
    juce::MidiBuffer midi;
    SmartCompAudioProcessor proc;
    proc.prepareToPlay(48000, 512);

    // Worst case: FET smash + max drive + deep threshold (vintage-style hot)
    setParam(proc, "threshold", -24.f);
    setParam(proc, "ratio", 20.f);
    setParam(proc, "attack", 0.05f);
    setParam(proc, "release", 80.f);
    setParam(proc, "knee", 0.f);
    setParam(proc, "makeup", 4.f);
    setParam(proc, "mix", 70.f);
    setParam(proc, "drive", 12.f);
    setChoice(proc, "circuit", 3); // FET

    // HOT broadband input: stacked sines + noise, peaks ~+2 dBFS
    const int N = 512 * 450; // ~4.5 s
    juce::AudioBuffer<float> src(2, N);
    unsigned st = 777;
    auto white = [&]() { st = st * 1664525u + 1013904223u; return ((st >> 16) / 32768.0f - 1.0f); };
    for (int i = 0; i < N; ++i)
    {
        double t = i / 48000.0;
        float v = 0.9f * std::sin(2*3.14159*110*t) + 0.7f * std::sin(2*3.14159*220*t)
                + 0.4f * std::sin(2*3.14159*440*t) + white() * 0.25f;
        src.setSample(0, i, v); src.setSample(1, i, v * 0.9f);
    }

    proc.requestLearn();
    float worst = 0;
    bool bad = false;
    juce::AudioBuffer<float> blk(2, 512);
    for (int off = 0; off < N; off += 512)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i) blk.setSample(ch, i, src.getSample(ch, off + i));
        proc.processBlock(blk, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                float s = blk.getSample(ch, i);
                if (!std::isfinite(s)) bad = true;
                worst = std::max(worst, std::abs(s));
            }
    }
    float worstDB = 20 * std::log10(worst + 1e-9f);
    printf("learn-run worst peak: %.2f dBFS\n", worstDB);
    CHECK(!bad, "learn run: all samples finite");
    CHECK(worstDB <= -0.3f, "learn run: never clips (worst %.2fdBFS)", worstDB);

    auto res = proc.takeLearnResult();
    CHECK(res.ready && res.features.valid, "learn result ready with valid features");
    CHECK(res.thresholdDB > -60.0 && res.thresholdDB <= 0.0, "learned threshold sane (%.1fdB)", res.thresholdDB);
    printf("learned: %s conf %.0f%% thr %.1f\n", AutoLeveler::className(res.cls),
           res.confidence * 100, res.thresholdDB);

    printf(fails == 0 ? "LEARN ANTI-CLIP TESTS PASSED\n" : "FAILURES: %d\n", fails);
    return fails;
}
