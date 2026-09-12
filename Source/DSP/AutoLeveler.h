#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// SmartComp AutoLeveler: hears WHERE the plugin is inserted, figures out WHAT
// is playing (kick, snare, drums, bass, guitar, keys, strings, vocal or full
// mix - instruments and human voices), then drives the compressor for you.
//
// Two workflows:
//   LEARN  - one-shot 4 s capture: classifies, then sets threshold (relative
//            to the measured level), ratio, attack, release, knee and SCF.
//   AUTO   - continuous: the threshold rides the slow RMS (level riding) with
//            a GR-cap servo, so dynamics stay regulated hands-free.
//
// The user picks a profile (or Auto-Detect) to help; the engine does the rest.
class AutoLeveler
{
public:
    enum class SourceClass
    {
        Kick = 0, Snare, Drums, Bass, Guitar, Keys, Strings, Vocal, Mix, Unknown,
        NumClasses
    };

    static const char* className(SourceClass c);
    static juce::StringArray classNames(); // for profile combo (index 0 = Auto-Detect)

    struct Features
    {
        double rmsDB = -100;       // slow RMS level
        double peakDB = -100;      // fast peak envelope
        double crestDB = 0;        // peak - rms
        double dynRangeDB = 0;     // peak envelope - noise floor
        double centroidHz = 1000;  // spectral centroid (5-band estimate)
        double zcr = 0;            // zero-crossing rate 0..1 (gated)
        double transientRate = 0;  // onsets per second (4 s window)
        double subRatio = 0;       // <150 Hz energy share
        double presenceRatio = 0;  // 2..6 kHz energy share
        double airRatio = 0;       // >6 kHz energy share
        double spectralSpread = 1; // max band share / mean share (peaky vs flat)
        bool valid = false;        // enough level to judge
    };

    struct ClassResult
    {
        SourceClass cls = SourceClass::Unknown;
        float confidence = 0.0f; // 0..1 margin over runner-up
        float scores[(int)SourceClass::NumClasses] = { 0 };
    };

    // Target compressor setup for a class. Threshold is RELATIVE: the final
    // threshold = measured slow RMS + thrOffsetDB (level riding).
    struct AutoParams
    {
        double thrOffsetDB = 4.0; // dB above slow RMS
        double ratio = 2.0;
        double attackMs = 10.0;
        double releaseMs = 250.0;
        bool autoRelease = true;
        double kneeDB = 6.0;
        double scHPF = 40.0;
        double maxGRdB = 6.0;     // GR-cap servo ceiling
    };

    static AutoParams paramsForClass(SourceClass c);

    AutoLeveler();

    void prepare(double sampleRate);
    void reset();

    // Feed stereo (or mono via R == nullptr) every block
    void pushBlock(const float* L, const float* R, int numSamples);

    // Snapshot of current features (UI thread safe: copies under lock)
    Features getFeatures() const;

    // Classify features. prior = user profile (Unknown = free Auto-Detect).
    ClassResult classify(const Features& f, SourceClass prior = SourceClass::Unknown) const;

    // ---- Continuous AUTO mode (called per block from the audio thread) ----
    // Returns the smoothed threshold to use this block. Dual-rate riding:
    // threshold drops fast (150 ms) when more control is needed, releases
    // control slowly (1.5 s), and snaps on huge gaps (e.g. transport start)
    // so playback never fades in.
    double updateAuto(double currentThresholdDB, int numSamples,
                      SourceClass lockedProfile, double& outAvgGR);
    void noteGainReduction(float grDB); // call per block with block max GR

    // ---- LEARN support ----
    void beginLearn();            // resets the 4 s capture window
    bool learnReady() const;      // 4 s captured?
    float learnProgress() const;  // 0..1

    double getSampleRate() const { return sampleRate; }

private:
    static float membership(float x, float a, float b, float c, float d); // trapezoid 0..1

    double sampleRate = 48000.0;

    // Envelopes
    double peakEnv = 0.0;   // fast peak (linear)
    double peakHold = 0.0;  // peak with slow release: phase-stable crest/dyn
    double rmsEnv = 1e-12;  // slow mean-square
    double floorEnv = 1e-9; // slow minimum tracker (quiet passages)
    double slowRMS = 1e-9;  // ~1.2 s RMS for level riding

    // 4 one-pole lowpasses -> 5 energy bands
    double lp150 = 0, lp800 = 0, lp2000 = 0, lp6000 = 0;
    double eSub = 0, eLowMid = 0, eMid = 0, ePres = 0, eAir = 0;
    static double lpCoeff(double cutoff, double sr);

    // ZCR + onset detection
    double zcrCount = 0, zcrTotal = 0;
    double fastRMS = 0, slowRMSdet = 0;
    int refractory = 0;
    static constexpr int kOnsetWindow = 48000 * 4;
    int onsetBuf[kOnsetWindow / 64] = { 0 }; // coarse onset flags
    int onsetWrite = 0, onsetCount = 0;
    int blockCounter = 0;
    float lastSample = 0.0f;

    // AUTO servo state
    double thrSmooth = -18.0;
    double avgGR = 0.0;
    double grOverTime = 0.0;
    bool thrInit = false;
    int validBlocks = 0; // blocks since audio became valid: priming window

    // LEARN capture
    bool learning = false;
    int learnSamples = 0;
    static constexpr int kLearnSamples = 48000 * 4;

    mutable juce::CriticalSection lock;
};
