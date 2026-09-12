#pragma once
#include <JuceHeader.h>

// SongDynamics: impara il livello e il crest di ogni battuta del brano.
//
// - In LEARN (transport in play + ppq valida) misura RMS/picco per battuta.
// - In FOLLOW il processor fa "riding" della threshold sul livello della
//   battuta corrente (thrEff = userThr + barRMS - songAvgRMS) e adatta il
//   ratio al crest (piu' transienti -> piu' controllo), con glide.
// - La mappa e' persistita nello stato del plugin.
class SongDynamics
{
public:
    static constexpr int MaxBars = 1024;

    struct BarEntry
    {
        int bar = -1;
        float rmsDB = -60.0f;
        float peakDB = -60.0f;
        float crestDB = 0.0f;
        bool silent = true; // sotto -60 dB: pause, non materiale da riding
    };

    SongDynamics();

    void prepare(double sampleRate, int beatsPerBar = 4);
    void reset();
    void flush(); // consolida la battuta corrente (chiamare su stop del transport)
    void setBeatsPerBar(int bpb);
    void setLearning(bool shouldLearn) { learning.store(shouldLearn); }
    bool isLearning() const { return learning.load(); }

    // mono/stereo pre-compressione. Solo audio thread.
    void pushAudioBlock(const float* L, const float* R, int numSamples,
                        double ppqPosition, bool ppqValid, bool isPlaying,
                        double bpm, double sampleRate);

    bool getEntryForPpq(double ppqPosition, BarEntry& out) const;
    bool getEntryForBar(int bar, BarEntry& out) const;
    bool getAverages(float& avgRmsDB, float& avgCrestDB) const;
    int getNumBars() const;
    bool hasData() const;
    int getCurrentBar() const { return currentBar; }
    juce::String getStatusText() const;

    juce::ValueTree toValueTree() const;
    void restoreFromValueTree(const juce::ValueTree& v);

private:
    int barFromPpq(double ppq) const;
    void finalizeCurrentBar();
    int nearestMusicalBar(double ppqPosition, bool usePpq, int bar) const; // lock; -1 se solo silenzio

    mutable juce::CriticalSection lock;
    double sampleRate = 48000.0;
    int beatsPerBar = 4;
    std::atomic<bool> learning { false };

    std::vector<BarEntry> bars;
    int currentBar = -1;
    double barSumSquares = 0.0;
    float barPeak = 0.0f;
    juce::int64 barSampleCount = 0;
};
