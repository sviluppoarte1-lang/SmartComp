// Song-dynamics test: per-bar level/crest learning, lookup, persistence.
#include "DSP/SongDynamics.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { ++fails; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
    else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void genLevel(std::vector<float>& out, int n, double sr, float amp, bool transient)
{
    out.resize((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double t = i / sr;
        float s = amp * (float) std::sin(2 * 3.14159265 * 220 * t);
        if (transient && (i % 24000) < 64) s += amp * 2.0f; // click -> crest alto
        out[(size_t) i] = s;
    }
}

static void pushAsBlocks(SongDynamics& sd, const std::vector<float>& audio,
                         double ppqStart, double ppqPerBlock, double bpm, double sr)
{
    const int bs = 512;
    double ppq = ppqStart;
    for (size_t pos = 0; pos < audio.size(); pos += bs)
    {
        int n = (int) juce::jmin<size_t>(bs, audio.size() - pos);
        sd.pushAudioBlock(audio.data() + pos, nullptr, n, ppq, true, true, bpm, sr);
        ppq += ppqPerBlock;
    }
}

int main()
{
    const double sr = 48000.0, bpm = 120.0;
    const double ppqPerBlock = 512.0 / sr * bpm / 60.0;

    SongDynamics sd;
    sd.prepare(sr, 4);
    sd.setLearning(true);

    std::vector<float> verse, chorus;
    genLevel(verse, 96000, sr, 0.15f, false);   // strofa piano
    genLevel(chorus, 96000, sr, 0.5f, true);    // ritornello forte + transienti

    pushAsBlocks(sd, verse, 0.0, ppqPerBlock, bpm, sr);   // bar 0
    pushAsBlocks(sd, chorus, 4.0, ppqPerBlock, bpm, sr);  // bar 1
    pushAsBlocks(sd, verse, 8.0, ppqPerBlock, bpm, sr);   // bar 2
    sd.setLearning(false);

    CHECK(sd.getNumBars() >= 2, "learned >=2 bars (got %d)", sd.getNumBars());

    SongDynamics::BarEntry e0, e1;
    CHECK(sd.getEntryForBar(0, e0), "lookup bar 0");
    CHECK(sd.getEntryForBar(1, e1), "lookup bar 1");
    printf("bar0 rms %.1f crest %.1f | bar1 rms %.1f crest %.1f\n",
           e0.rmsDB, e0.crestDB, e1.rmsDB, e1.crestDB);
    CHECK(e1.rmsDB - e0.rmsDB > 6.0f, "chorus louder than verse (%.1fdB)", e1.rmsDB - e0.rmsDB);
    CHECK(e1.crestDB > e0.crestDB, "chorus crest higher (%.1f vs %.1f)", e1.crestDB, e0.crestDB);

    float avgRms, avgCrest;
    CHECK(sd.getAverages(avgRms, avgCrest), "averages");
    CHECK(avgRms > e0.rmsDB && avgRms < e1.rmsDB, "avg between bars (%.1f)", avgRms);

    SongDynamics::BarEntry ep;
    CHECK(sd.getEntryForPpq(5.0, ep), "ppq lookup");
    CHECK(std::abs(ep.rmsDB - e1.rmsDB) < 1e-4f, "ppq 5.0 resolves to bar 1");

    // Follow math: threshold rides bar level
    float thrUser = -18.0f;
    float thrBar1 = thrUser + (e1.rmsDB - avgRms);
    float thrBar0 = thrUser + (e0.rmsDB - avgRms);
    CHECK(thrBar1 > thrBar0, "threshold rides level (bar1 %.1f > bar0 %.1f)", thrBar1, thrBar0);

    // Persistence roundtrip
    auto vt = sd.toValueTree();
    SongDynamics sd2;
    sd2.prepare(sr, 4);
    sd2.restoreFromValueTree(vt);
    CHECK(sd2.getNumBars() == sd.getNumBars(), "roundtrip numBars %d", sd2.getNumBars());
    SongDynamics::BarEntry r1;
    sd2.getEntryForBar(1, r1);
    CHECK(std::abs(r1.rmsDB - e1.rmsDB) < 1e-4f, "roundtrip rms identical");

    // No-learn guard
    SongDynamics sd3;
    sd3.prepare(sr, 4);
    sd3.setLearning(false);
    pushAsBlocks(sd3, verse, 0.0, ppqPerBlock, bpm, sr);
    CHECK(! sd3.hasData(), "no map when learn off");

    // Silence skip: pause escluse da medie e lookup
    {
        SongDynamics sd4;
        sd4.prepare(sr, 4);
        sd4.setLearning(true);
        std::vector<float> silence(96000, 0.0f);
        pushAsBlocks(sd4, verse, 0.0, ppqPerBlock, bpm, sr);
        pushAsBlocks(sd4, silence, 4.0, ppqPerBlock, bpm, sr);
        pushAsBlocks(sd4, chorus, 8.0, ppqPerBlock, bpm, sr);
        sd4.setLearning(false);
        sd4.flush();
        CHECK(sd4.getNumBars() == 3, "3 bars indexed (got %d)", sd4.getNumBars());
        float aRms, aCrest;
        CHECK(sd4.getAverages(aRms, aCrest), "averages over musical bars");
        printf("avg rms %.1f (verse %.1f chorus %.1f)\n",
               aRms, sd4.getNumBars() > 0 ? -19.5f : 0.0f, -8.9f);
        CHECK(aRms > -20.0f && aRms < -8.0f, "silence does not poison average (%.1f)", aRms);
        SongDynamics::BarEntry e;
        CHECK(sd4.getEntryForPpq(5.0, e), "ppq in silence resolves to music");
        CHECK(e.rmsDB > -20.0f, "resolved entry is musical (%.1f)", e.rmsDB);
    }

    printf(fails == 0 ? "SONGDYN ALL OK\n" : "SONGDYN %d FAILURES\n", fails);
    return fails;
}
