// End-to-end: vero SmartComp su drums.wav con stub transport a 126bpm.
// Pass 1: default (no song). Pass 2: LEARN. Pass 3: FOLLOW.
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

struct StubPlayHead : juce::AudioPlayHead
{
    juce::AudioPlayHead::PositionInfo pos;
    double sampleRate = 44100.0, bpm = 126.0;
    juce::int64 samples = 0;
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override { return pos; }
    void reset()
    {
        samples = 0;
        pos.setIsPlaying(true);
        pos.setBpm(bpm);
        pos.setPpqPosition(0.0);
        pos.setTimeInSeconds(0.0);
    }
    void advance(int n)
    {
        samples += n;
        double t = samples / sampleRate;
        pos.setTimeInSeconds(t);
        pos.setPpqPosition(t * bpm / 60.0);
    }
};

static void setParam(SmartCompAudioProcessor& p, const char* id, float v)
{
    if (auto* par = p.apvts.getParameter(id))
        par->setValueNotifyingHost(par->convertTo0to1(v));
}

int main(int argc, char** argv)
{
    bool defaultOnly = argc > 1 && std::string(argv[1]) == "default";
    const char* inPath = "/tmp/opencode/drums_mono.f32";
    const char* outPath = defaultOnly ? "/tmp/opencode/chain_comp_default.f32" : "/tmp/opencode/chain_comp.f32";
    bool useSong = ! defaultOnly;
    FILE* f = fopen(inPath, "rb");
    fseek(f, 0, SEEK_END);
    size_t n = (size_t) ftell(f) / sizeof(float);
    fseek(f, 0, SEEK_SET);
    std::vector<float> audio(n);
    [[maybe_unused]] size_t nr = fread(audio.data(), sizeof(float), n, f);
    fclose(f);

    SmartCompAudioProcessor proc;
    proc.prepareToPlay(44100, 512);
    StubPlayHead ph;
    proc.setPlayHead(&ph);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf(2, 512);

    auto runPass = [&](bool learn, bool follow, std::vector<float>& out, const char* tag) {
        ph.reset();
        setParam(proc, "songLearn", learn ? 1.0f : 0.0f);
        setParam(proc, "songFollow", follow ? 1.0f : 0.0f);
        out.assign(n, 0);
        for (size_t pos = 0; pos + 512 <= n; pos += 512)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample(ch, i, audio[pos + i]);
            proc.processBlock(buf, midi);
            for (int i = 0; i < 512; ++i) out[pos + i] = buf.getSample(0, i);
            ph.advance(512);
        }
        printf("%s: %s effThr=%.1f\n", tag,
               proc.getSongDynamics().getStatusText().toRawUTF8(), proc.getEffectiveThreshold());
    };

    std::vector<float> out;
    if (! useSong)
    {
        runPass(false, false, out, "DEFAULT");
    }
    else
    {
        runPass(true, false, out, "LEARN");
        runPass(false, true, out, "FOLLOW");
    }
    FILE* o = fopen(outPath, "wb");
    fwrite(out.data(), sizeof(float), n, o);
    fclose(o);

    auto sec = [&](const std::vector<float>& v, int s0, int s1) {
        double ss = 0; float pk = 0;
        size_t a = (size_t) s0 * 44100, b = juce::jmin(n, (size_t) s1 * 44100);
        for (size_t i = a; i < b; ++i) { ss += (double) v[i] * v[i]; pk = juce::jmax(pk, std::abs(v[i])); }
        double rms = 20 * log10(sqrt(ss / (b - a)) + 1e-12);
        return std::make_pair(rms, 20 * log10(pk + 1e-12));
    };
    for (auto [a, b] : { std::make_pair(20, 35), std::make_pair(150, 165), std::make_pair(240, 255) })
    {
        auto [ri, pi] = sec(audio, a, b);
        auto [ro, po] = sec(out, a, b);
        printf("  %d-%ds: rms %+.1f -> %+.1f (d=%+.1f) peak %+.1f -> %+.1f (d=%+.1f) crest %+.1f -> %+.1f\n",
               a, b, ri, ro, ro - ri, pi, po, po - pi, pi - ri, po - ro);
    }
    return 0;
}
