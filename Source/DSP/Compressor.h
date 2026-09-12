#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// SmartComp core: feed-forward stereo-linked compressor + brickwall limiter
// with 6 historic circuit models. Inspired by the classics (our own design,
// no copied code or curves):
//   Modern  - transparent, precise (clean)
//   VariMu  - Fairchild-style: progressive ratio that grows with depth,
//             lazy release, tube warmth
//   Opto    - LA-2A-style: gentle catch, two-stage release (fast then
//             very slow tail), tube glow
//   FET     - 1176-style: ultra-fast attack, aggressive knee, edgy grit
//   VCA     - SSL/dbX-style bus glue: RMS-leaning, punchy, transformer sheen
//   Diode   - Neve-style bridge: smooth thick leveling, warm saturation
struct CompParams
{
    double thresholdDB = -18.0; // -60..0
    double ratio = 2.0;         // 1..20 (>=19.5 treated as inf)
    double attackMs = 10.0;     // 0.02..300 (FET models reach the bottom)
    double releaseMs = 200.0;   // 20..3000
    bool   autoRelease = true;  // program-dependent release
    double kneeDB = 6.0;        // 0..12
    double makeupDB = 0.0;      // manual makeup 0..24
    bool   autoMakeup = true;   // match output loudness to input
    double mix = 1.0;           // 0 dry .. 1 wet (parallel)
    double scHPF = 20.0;        // sidechain high-pass Hz (<=20 = off)
    double detectorMix = 0.5;   // 0 = peak .. 1 = RMS
    bool   limiter = false;     // brickwall mode (inf ratio + ceiling)
    double ceilingDB = -0.5;    // limiter ceiling -12..0
    int    circuitModel = 0;    // 0 Modern, 1 VariMu, 2 Opto, 3 FET, 4 VCA, 5 Diode
    double driveDB = 0.0;       // saturation drive 0..24 (on top of model floor)
};

struct CircuitModelInfo
{
    enum Model { Modern = 0, VariMu, Opto, FET, VCA, Diode, NumModels };
    static const char* name(int m)
    {
        switch (m)
        {
            case Modern: return "Modern";
            case VariMu: return "Vari-Mu";
            case Opto:   return "Opto";
            case FET:    return "FET";
            case VCA:    return "VCA";
            case Diode:  return "Diode";
            default:     return "?";
        }
    }
    static juce::StringArray names()
    {
        return { "Modern", "Vari-Mu", "Opto", "FET", "VCA", "Diode" };
    }
    // Minimum saturation drive per model (dB): the circuit always has a color
    static float driveFloor(int m)
    {
        switch (m)
        {
            case VariMu: return 4.0f;
            case Opto:   return 2.5f;
            case FET:    return 5.0f;
            case VCA:    return 1.5f;
            case Diode:  return 3.5f;
            default:     return 0.0f;
        }
    }
};

// Final safety net: transparent brickwall that ONLY acts on overs.
// Peak follower (0.05 ms attack, 40 ms release) at -0.5 dBFS. Below the
// ceiling gain is exactly 1.0 (bit-transparent); nothing else is touched.
struct SafetyLimiter
{
    void prepare(double sr) { sampleRate = sr > 8000 ? sr : 48000.0; reset(); }
    void reset() { peak = 0.0f; }

    inline float process(float x) noexcept
    {
        constexpr float ceilLin = 0.94406088f; // -0.5 dBFS
        const float a = std::abs(x);
        const float atk = 1.0f - std::exp(-1.0f / (float)(0.00002 * sampleRate)); // 0.02 ms
        const float rel = 1.0f - std::exp(-1.0f / (float)(0.040 * sampleRate));   // 40 ms
        peak += ((a > peak) ? atk : rel) * (a - peak);
        const float g = (peak > ceilLin) ? ceilLin / juce::jmax(peak, 1e-9f) : 1.0f;
        const float y = x * g;
        // Hard clamp: the follower needs ~1 sample to catch ultrafast spikes;
        // the clamp (engaging only there) makes the ceiling airtight.
        return juce::jlimit(-ceilLin, ceilLin, y);
    }

    double sampleRate = 48000.0;
    float peak = 0.0f;
};

class Compressor
{
public:
    Compressor();

    void prepare(double sampleRate);
    void reset();
    void setParams(const CompParams& p); // cheap: recomputes coeffs only on change

    // Stereo in-place (.linked detector on max(|L|,|R|)). Mono: pass same pointer twice or 1ch handled.
    void processBlock(juce::AudioBuffer<float>& buffer);

    // Meters (written on audio thread, read on UI thread)
    float getGainReductionDB() const { return grMeter.load(); }
    float getInputLevelDB() const { return inMeter.load(); }
    float getOutputLevelDB() const { return outMeter.load(); }

    // Static gain computer (unit-testable): returns POSITIVE reduction dB for a static level
    static float staticGainReduction(float levelDB, float thrDB, float ratio, float kneeDB);

    double getSampleRate() const { return sampleRate; }
    const CompParams& getParams() const { return params; }

private:
    float msToCoeff(float ms) const; // one-pole per-sample coefficient

    CompParams params;
    double sampleRate = 48000.0;

    // Detector state
    float peakEnv = 0.0f;
    float rmsAvg = 0.0f;   // fixed short-term mean square (unbiased)
    float rmsSm = 0.0f;    // ballistics-smoothed RMS level
    float rmsAvgC = 0.001f;
    float grSmooth = 0.0f; // smoothed POSITIVE reduction dB
    float grSlow = 0.0f;   // opto-style slow tail (two-stage release)
    float mkSmooth = 0.0f; // auto-makeup follower (dB)

    // Vintage saturation (wet path only, parallel dry stays clean)
    static float saturate(float x, float driveGain, int model);

    // Sidechain HPF (2nd order biquad, stereo-independent states via Transposed DF2)
    struct HpState { float s1 = 0, s2 = 0; };
    float scB0=1, scB1=0, scB2=0, scA1=0, scA2=0;
    HpState scL, scR;
    void updateSidechainFilter();
    inline float scFilter(float x, HpState& st) noexcept
    {
        float out = scB0 * x + st.s1;
        st.s1 = scB1 * x - scA1 * out + st.s2;
        st.s2 = scB2 * x - scA2 * out;
        return out;
    }

    std::atomic<float> grMeter { 0.0f };
    std::atomic<float> inMeter { -100.0f };
    std::atomic<float> outMeter { -100.0f };
};
