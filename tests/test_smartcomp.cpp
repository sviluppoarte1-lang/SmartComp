// SmartComp DSP verification: compressor, classifier, circuits, presets.
// Standalone harness - no plugin/host needed for DSP parts.
// (No licensing in the public build: full version, always unlocked.)
#include "DSP/Compressor.h"
#include "DSP/AutoLeveler.h"
#include "Presets/CompPresetManager.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { ++fails; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
    else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static double sr = 48000.0;

struct Sig { std::vector<float> L, R; int n() const { return (int)L.size(); } };

static Sig render(double seconds, std::function<float(double t, int ch)> fn, bool stereo = false)
{
    int N = (int)(seconds * sr);
    Sig s; s.L.resize(N); s.R.resize(N);
    for (int i = 0; i < N; ++i) { double t = i / sr; s.L[i] = fn(t, 0); s.R[i] = stereo ? fn(t, 1) : fn(t, 0); }
    return s;
}

static unsigned rngState = 12345;
static float white() { rngState = rngState * 1664525u + 1013904223u; return ((rngState >> 16) / 32768.0f - 1.0f); }

static void feedLeveler(AutoLeveler& lv, const Sig& s)
{
    int N = s.n();
    for (int i = 0; i < N; i += 512)
    {
        int n = std::min(512, N - i);
        lv.pushBlock(s.L.data() + i, s.R.data() + i, n);
    }
}

static const char* clsName(AutoLeveler::SourceClass c) { return AutoLeveler::className(c); }

int main()
{
    using SC = AutoLeveler::SourceClass;

    // ---------- static gain computer ----------
    CHECK(std::abs(Compressor::staticGainReduction(-10, -20, 4, 0) - 7.5f) < 1e-4, "hard-knee 7.5dB GR");
    CHECK(Compressor::staticGainReduction(-30, -20, 4, 0) == 0.f, "below threshold = 0 GR");
    CHECK(std::abs(Compressor::staticGainReduction(-10, -20, 20, 0) - 10.f) < 1e-4, "inf ratio clamps to threshold");
    {
        float below = Compressor::staticGainReduction(-23.1f, -20, 4, 6);
        float edgeLo = Compressor::staticGainReduction(-23.0f, -20, 4, 6);
        float edgeHi = Compressor::staticGainReduction(-17.0f, -20, 4, 6);
        float above = Compressor::staticGainReduction(-16.9f, -20, 4, 6);
        CHECK(below == 0.f && edgeLo < 0.01f, "soft-knee lower edge ~0 (%.3f, %.3f)", below, edgeLo);
        CHECK(std::abs(edgeHi - 2.25f) < 0.01f && std::abs(above - 2.325f) < 0.05f,
              "soft-knee upper edge continuous (%.3f -> %.3f)", edgeHi, above);
    }

    // ---------- compressor on sine ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -20; p.ratio = 4; p.attackMs = 1; p.releaseMs = 100;
        p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.makeupDB = 0;
        p.mix = 1; p.scHPF = 20; p.detectorMix = 1.0f; p.limiter = false;
        c.setParams(p);
        auto s = render(3.0, [](double t, int) { return 0.4472f * std::sin(2 * 3.14159 * 220 * t); }, true);
        juce::AudioBuffer<float> buf(2, s.n());
        for (int i = 0; i < s.n(); ++i) { buf.setSample(0, i, s.L[i]); buf.setSample(1, i, s.R[i]); }
        c.processBlock(buf);
        float gr = c.getGainReductionDB();
        CHECK(gr > 6.0f && gr < 9.0f, "sine GR ~7.5dB (got %.2f)", gr);
        bool bad = false;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < buf.getNumSamples(); ++i)
            if (!std::isfinite(buf.getSample(ch, i))) bad = true;
        CHECK(!bad, "no NaN/Inf after compression");
    }

    // ---------- transport start: no fade-in dip ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -18; p.ratio = 3; p.attackMs = 3; p.releaseMs = 180;
        p.autoRelease = true; p.kneeDB = 4; p.autoMakeup = true; p.mix = 1;
        p.detectorMix = 0.5f; p.circuitModel = CircuitModelInfo::Modern; p.driveDB = 0;
        c.setParams(p);
        auto s = render(3.0, [](double t, int) { return 0.3566f * std::sin(2 * 3.14159 * 220 * t); }, true);
        juce::AudioBuffer<float> buf(2, s.n());
        for (int i = 0; i < s.n(); ++i) { buf.setSample(0, i, s.L[i]); buf.setSample(1, i, s.R[i]); }
        c.processBlock(buf);
        auto rmsDB = [&](int from, int n) {
            double sum = 0;
            for (int i = from; i < from + n; ++i) { float v = buf.getSample(0, i); sum += v * v; }
            return 10 * std::log10(sum / n + 1e-12);
        };
        double first = rmsDB(256, 3072);
        double settled = rmsDB(s.n() - 24000, 24000);
        printf("transport start: first %.2fdB vs settled %.2fdB\n", first, settled);
        CHECK(std::abs(first - settled) < 2.5, "no start dip/fade-in (diff %.2fdB)", std::abs(first - settled));
    }

    // ---------- mix=0 dry ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -40; p.ratio = 10; p.mix = 0; p.detectorMix = 0;
        c.setParams(p);
        auto s = render(1.0, [](double t, int) { return 0.3f * std::sin(2 * 3.14159 * 440 * t); });
        juce::AudioBuffer<float> buf(1, s.n());
        for (int i = 0; i < s.n(); ++i) buf.setSample(0, i, s.L[i]);
        c.processBlock(buf);
        bool exact = true;
        for (int i = 0; i < s.n(); ++i) if (buf.getSample(0, i) != s.L[i]) exact = false;
        CHECK(exact, "mix=0 bit-exact dry");
    }

    // ---------- auto makeup matches loudness ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -30; p.ratio = 4; p.attackMs = 2; p.releaseMs = 150;
        p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = true; p.mix = 1; p.detectorMix = 1.0f;
        c.setParams(p);
        auto s = render(4.0, [](double t, int) { return 0.1413f * std::sin(2 * 3.14159 * 110 * t); });
        juce::AudioBuffer<float> buf(1, s.n());
        for (int i = 0; i < s.n(); ++i) buf.setSample(0, i, s.L[i]);
        float inRMS = buf.getRMSLevel(0, 0, s.n());
        c.processBlock(buf);
        float outRMS = buf.getRMSLevel(0, s.n() / 2, s.n() / 2);
        float diff = std::abs(20 * std::log10(outRMS + 1e-9f) - 20 * std::log10(inRMS + 1e-9f));
        CHECK(diff < 2.0f, "auto-makeup loudness match (diff %.2fdB)", diff);
    }

    // ---------- limiter ceiling (hot input, slow attack: only clamp stops it) ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -20; p.ratio = 20; p.attackMs = 5.f; p.releaseMs = 100;
        p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.mix = 1;
        p.detectorMix = 0; p.limiter = true; p.ceilingDB = -1.0;
        c.setParams(p);
        auto s = render(2.0, [](double t, int) { return 2.0f * std::sin(2 * 3.14159 * 100 * t); }, true);
        juce::AudioBuffer<float> buf(2, s.n());
        for (int i = 0; i < s.n(); ++i) { buf.setSample(0, i, s.L[i]); buf.setSample(1, i, s.R[i]); }
        c.processBlock(buf);
        float peak = buf.getMagnitude(0, 0, s.n());
        float peakDB = 20 * std::log10(peak + 1e-9f);
        CHECK(peakDB <= -0.9f, "limiter ceiling respected (peak %.2fdBFS)", peakDB);
    }

    // ---------- silence stability ----------
    {
        Compressor c; c.prepare(sr);
        CompParams p; c.setParams(p);
        juce::AudioBuffer<float> buf(2, 2048); buf.clear();
        c.processBlock(buf);
        CHECK(std::isfinite(c.getGainReductionDB()), "silence: finite GR meter");
    }

    // ================= CLASSIFIER (mid-groove snapshots) =================
    auto classifySig = [&](const Sig& s, const char* tag) {
        AutoLeveler lv; lv.prepare(sr);
        int feedN = (s.n() * 3) / 4;
        for (int i = 0; i < feedN; i += 512)
        {
            int n = std::min(512, feedN - i);
            lv.pushBlock(s.L.data() + i, s.R.data() + i, n);
        }
        auto f = lv.getFeatures();
        auto r = lv.classify(f);
        printf("signal %-8s -> %-10s conf %.2f | rms %.1f crest %.1f dyn %.1f cent %.0f tr %.1f sub %.2f pres %.2f spread %.1f\n",
               tag, clsName(r.cls), r.confidence, f.rmsDB, f.crestDB, f.dynRangeDB,
               f.centroidHz, f.transientRate, f.subRatio, f.presenceRatio, f.spectralSpread);
        return r.cls;
    };

    auto kick = render(4.0, [](double t, int) {
        double beat = std::fmod(t, 0.5);
        float env = std::exp((float)-beat * 14.f);
        float v = std::sin(2 * 3.14159 * 55 * t) * env * 0.9f;
        if (beat < 0.003) v += white() * 0.5f * env;
        return v;
    });
    CHECK(classifySig(kick, "kick") == SC::Kick, "kick classified as Kick");

    auto snare = render(4.0, [](double t, int) {
        double beat = std::fmod(t, 0.5);
        float env = std::exp((float)-beat * 22.f);
        return (std::sin(2 * 3.14159 * 190 * t) * 0.30f + white() * 0.55f) * env;
    });
    CHECK(classifySig(snare, "snare") == SC::Snare, "snare classified as Snare");

    auto bass = render(4.0, [](double t, int) {
        return 0.35f * std::sin(2 * 3.14159 * 82.4 * t);
    });
    CHECK(classifySig(bass, "bass") == SC::Bass, "bass classified as Bass");

    auto vocal = render(4.0, [](double t, int) {
        float am = 0.65f + 0.35f * std::sin(2 * 3.14159 * 4.5 * t);
        float v = std::sin(2*3.14159*130*t) + 0.5f*std::sin(2*3.14159*260*t)
                + 0.3f*std::sin(2*3.14159*390*t) + 0.55f*std::sin(2*3.14159*2500*t);
        return v * 0.20f * am;
    });
    {
        auto cls = classifySig(vocal, "vocal");
        CHECK(cls == SC::Vocal, "vocal classified as Vocal (got %s)", clsName(cls));
    }

    auto mix = render(4.0, [](double t, int) {
        float v = 0.11f * std::sin(2*3.14159*220*t) + 0.10f * std::sin(2*3.14159*277.2*t)
                + 0.09f * std::sin(2*3.14159*329.6*t) + 0.08f * std::sin(2*3.14159*440*t)
                + 0.06f * std::sin(2*3.14159*880*t) + 0.05f * std::sin(2*3.14159*1760*t);
        v += white() * 0.035f;
        return v;
    });
    {
        auto cls = classifySig(mix, "mixbright");
        CHECK(cls == SC::Mix, "mix-like classified as Mix (got %s)", clsName(cls));
    }

    {
        AutoLeveler lv; lv.prepare(sr);
        juce::AudioBuffer<float> buf(1, 48000); buf.clear();
        for (int i = 0; i < 48000; i += 512) lv.pushBlock(buf.getReadPointer(0) + i, nullptr, 512);
        auto r = lv.classify(lv.getFeatures());
        CHECK(r.cls == SC::Unknown, "silence classified as Unknown");
    }

    // AUTO threshold priming
    {
        AutoLeveler lv; lv.prepare(sr);
        auto s = render(2.0, [](double t, int) { return 0.3566f * std::sin(2 * 3.14159 * 220 * t); });
        feedLeveler(lv, s);
        double thr = -40.0, avgGR = 0;
        for (int b = 0; b < 20; ++b) thr = lv.updateAuto(thr, 512, SC::Vocal, avgGR);
        auto f = lv.getFeatures();
        double target = f.rmsDB + 3.0;
        printf("auto prime @0.2s: thr %.2f target %.2f\n", thr, target);
        CHECK(std::abs(thr - target) < 6.0, "AUTO threshold primes fast (gap %.2fdB)", std::abs(thr - target));
        for (int b = 0; b < 40; ++b) thr = lv.updateAuto(thr, 512, SC::Vocal, avgGR);
        printf("auto prime @0.6s: thr %.2f target %.2f\n", thr, target);
        CHECK(std::abs(thr - target) < 2.0, "AUTO threshold settled (gap %.2fdB)", std::abs(thr - target));
    }

    // ================= CIRCUIT MODELS =================
    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -20; p.ratio = 2; p.attackMs = 2; p.releaseMs = 200;
        p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.mix = 1;
        p.detectorMix = 1.0f; p.circuitModel = CircuitModelInfo::VariMu; p.driveDB = 0;
        c.setParams(p);
        auto s = render(3.0, [](double t, int) { return 0.8913f * std::sin(2 * 3.14159 * 110 * t); });
        juce::AudioBuffer<float> buf(1, s.n());
        for (int i = 0; i < s.n(); ++i) buf.setSample(0, i, s.L[i]);
        c.processBlock(buf);
        float gr = c.getGainReductionDB();
        CHECK(gr > 9.5f, "varimu progressive ratio (GR %.2f > 8 linear)", gr);
    }

    {
        auto run = [&](int model) {
            Compressor c; c.prepare(sr);
            CompParams p; p.thresholdDB = -20; p.ratio = 4; p.attackMs = 1; p.releaseMs = 100;
            p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.mix = 1;
            p.detectorMix = 0; p.circuitModel = model; p.driveDB = 0;
            c.setParams(p);
            auto s = render(3.0, [](double t, int) -> double {
                if (t < 1.0) return 0.7 * std::sin(2 * 3.14159 * 150 * t);
                return 0.0;
            });
            auto seg = [&](int from, int to) {
                juce::AudioBuffer<float> b(1, to - from);
                for (int i = from; i < to; ++i) b.setSample(0, i - from, s.L[i]);
                c.processBlock(b);
                return c.getGainReductionDB();
            };
            int i1000 = (int)(1.0 * sr), i1300 = (int)(1.3 * sr), i2500 = (int)(2.5 * sr);
            seg(0, i1000);
            float grEarly = seg(i1000, i1300);
            float grLate = seg(i1300, i2500);
            return std::make_pair(grEarly, grLate);
        };
        auto opto = run(CircuitModelInfo::Opto);
        auto modern = run(CircuitModelInfo::Modern);
        printf("opto tail: early %.2f late %.2f | modern early %.2f late %.2f\n",
               opto.first, opto.second, modern.first, modern.second);
        CHECK(opto.first > 1.0f, "opto catches burst (GR %.2f)", opto.first);
        CHECK(opto.second > modern.second + 0.3f, "opto tail slower than modern (%.2f vs %.2f)",
              opto.second, modern.second);
    }

    {
        Compressor c; c.prepare(sr);
        CompParams p; p.thresholdDB = -20; p.ratio = 4; p.attackMs = 0.05f; p.releaseMs = 100;
        p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.mix = 1;
        p.detectorMix = 0; p.circuitModel = CircuitModelInfo::FET; p.driveDB = 0;
        c.setParams(p);
        juce::AudioBuffer<float> buf(1, 480);
        for (int i = 0; i < 480; ++i) buf.setSample(0, i, 0.5f);
        c.processBlock(buf);
        float gr = c.getGainReductionDB();
        CHECK(gr > 9.0f, "FET fast attack settles (GR %.2f)", gr);
    }

    {
        auto thd = [&](int model, float drive) {
            Compressor c; c.prepare(sr);
            CompParams p; p.thresholdDB = 0; p.ratio = 1; p.attackMs = 10; p.releaseMs = 200;
            p.autoRelease = false; p.kneeDB = 0; p.autoMakeup = false; p.mix = 1;
            p.detectorMix = 0; p.circuitModel = model; p.driveDB = drive;
            c.setParams(p);
            const int N = 48000;
            juce::AudioBuffer<float> buf(1, N);
            for (int i = 0; i < N; ++i) buf.setSample(0, i, 0.9f * std::sin(2 * 3.14159 * 220 * i / sr));
            c.processBlock(buf);
            auto goertzel = [&](float f) {
                double s1 = 0, s2 = 0; double w = 2 * 3.14159 * f / sr;
                double cw = std::cos(w);
                for (int i = N/2; i < N; ++i) { double s0 = buf.getSample(0, i) + 2*cw*s1 - s2; s2 = s1; s1 = s0; }
                return std::sqrt(s1*s1 + s2*s2 - s1*s2*cw);
            };
            double h1 = goertzel(220);
            double harm = 0;
            for (int h = 2; h <= 5; ++h) { double m = goertzel(220*h); harm += m*m; }
            return std::sqrt(harm) / (h1 + 1e-12);
        };
        double thdModern = thd(CircuitModelInfo::Modern, 0);
        double thdFET = thd(CircuitModelInfo::FET, 12);
        double thdTube = thd(CircuitModelInfo::VariMu, 8);
        printf("THD modern %.5f fet %.4f tube %.4f\n", thdModern, thdFET, thdTube);
        CHECK(thdModern < 0.002, "modern path clean (THD %.5f)", thdModern);
        CHECK(thdFET > 0.01, "FET drive adds harmonics (THD %.4f)", thdFET);
        CHECK(thdTube > 0.005, "tube drive adds harmonics (THD %.4f)", thdTube);
    }

    // ================= PRESETS (57: Vintage/Strings/Percussion/Voices/Mastering) =================
    {
        CompPresetManager pm;
        CHECK(pm.getNumPresets() >= 55, "preset count >= 55 (got %d)", pm.getNumPresets());
        bool allOk = true;
        for (int i = 0; i < pm.getNumPresets(); ++i)
        {
            auto& pr = pm.getPreset(i);
            Compressor c; c.prepare(sr);
            CompParams p = pr.params;
            if (p.circuitModel < 0 || p.circuitModel > 5) allOk = false;
            if (p.driveDB < 0 || p.driveDB > 24) allOk = false;
            c.setParams(p);
            juce::AudioBuffer<float> buf(2, 24000);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < 24000; ++n)
                    buf.setSample(ch, n, 0.25f * std::sin(2*3.14159*(110+ch*40)*n/sr) + 0.05f * std::sin(2*3.14159*3000*n/sr));
            c.processBlock(buf);
            for (int ch = 0; ch < 2 && allOk; ++ch)
                for (int n = 0; n < 24000; ++n)
                    if (!std::isfinite(buf.getSample(ch, n))) allOk = false;
            if (!std::isfinite(c.getGainReductionDB())) allOk = false;
        }
        CHECK(allOk, "all presets run finite audio");
        auto cats = pm.getCategories();
        CHECK(cats.contains("Vintage") && cats.contains("Strings") && cats.contains("Percussion")
              && cats.contains("Vocals") && cats.contains("Mastering"),
              "categories cover Vintage/Strings/Percussion/Vocals/Mastering");
        int mastering = 0;
        for (int i = 0; i < pm.getNumPresets(); ++i)
            if (pm.getPreset(i).category == "Mastering") ++mastering;
        CHECK(mastering >= 8, "mastering presets >= 8 (got %d)", mastering);
    }

    // ================= SAFETY LIMITER =================
    {
        SafetyLimiter sl; sl.prepare(sr);
        CHECK(sl.process(0.25f) == 0.25f, "safety transparent below ceiling");
        SafetyLimiter sl2; sl2.prepare(sr);
        float peak = 0;
        for (int i = 0; i < 48000; ++i)
        {
            float y = sl2.process(2.0f * std::sin(2 * 3.14159 * 100 * i / sr));
            peak = std::max(peak, std::abs(y));
        }
        float peakDB = 20 * std::log10(peak + 1e-9f);
        CHECK(peakDB <= -0.4f, "safety ceiling holds hot signal (peak %.2fdBFS)", peakDB);
    }

    printf(fails == 0 ? "\nALL SMARTCOMP DSP TESTS PASSED\n" : "\nFAILURES: %d\n", fails);
    return fails;
}
