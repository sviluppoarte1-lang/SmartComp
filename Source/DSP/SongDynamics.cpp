#include "SongDynamics.h"

SongDynamics::SongDynamics()
{
    bars.reserve(256);
}

void SongDynamics::prepare(double sr, int bpb)
{
    juce::ScopedLock sl(lock);
    if (sr >= 8000) sampleRate = sr;
    if (bpb >= 1 && bpb <= 12) beatsPerBar = bpb;
}

void SongDynamics::reset()
{
    juce::ScopedLock sl(lock);
    bars.clear();
    currentBar = -1;
    barSumSquares = 0.0;
    barPeak = 0.0f;
    barSampleCount = 0;
}

void SongDynamics::flush()
{
    juce::ScopedLock sl(lock);
    if (currentBar >= 0 && barSampleCount > 0)
    {
        finalizeCurrentBar();
        barSumSquares = 0.0;
        barPeak = 0.0f;
        barSampleCount = 0;
    }
}

void SongDynamics::setBeatsPerBar(int bpb)
{
    juce::ScopedLock sl(lock);
    if (bpb >= 1 && bpb <= 12) beatsPerBar = bpb;
}

int SongDynamics::barFromPpq(double ppq) const
{
    if (ppq < 0) return 0;
    int bpb = juce::jmax(1, beatsPerBar);
    return (int) std::floor(ppq / (double) bpb);
}

void SongDynamics::pushAudioBlock(const float* L, const float* R, int numSamples,
                                  double ppqPosition, bool ppqValid, bool isPlaying,
                                  double bpm, double sr)
{
    if (L == nullptr || numSamples <= 0) return;
    if (! learning.load()) return;
    if (! isPlaying) return;

    juce::ScopedLock sl(lock);
    if (sr >= 8000 && std::abs(sr - sampleRate) > 1.0)
        sampleRate = sr;

    int bar = currentBar;
    if (ppqValid && bpm > 0)
        bar = barFromPpq(ppqPosition);
    if (bar < 0) bar = 0;
    if (bar >= MaxBars) return;

    if (bar != currentBar)
    {
        if (currentBar >= 0)
            finalizeCurrentBar();
        currentBar = bar;
        barSumSquares = 0.0;
        barPeak = 0.0f;
        barSampleCount = 0;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float a = std::abs(L[i]);
        if (R != nullptr) a = juce::jmax(a, std::abs(R[i]));
        if (! std::isfinite(a)) a = 0.0f;
        barSumSquares += (double) a * (double) a;
        if (a > barPeak) barPeak = a;
    }
    barSampleCount += numSamples;
}

void SongDynamics::finalizeCurrentBar()
{
    // Richiede `lock` acquisito.
    if (currentBar < 0 || currentBar >= MaxBars) return;

    static constexpr float kSilenceRmsDB = -60.0f;
    BarEntry entry;
    entry.bar = currentBar;
    if (barSampleCount > 256)
    {
        double meanSq = barSumSquares / (double) barSampleCount;
        entry.rmsDB = juce::Decibels::gainToDecibels((float) std::sqrt(meanSq) + 1e-9f);
        entry.peakDB = juce::Decibels::gainToDecibels(barPeak + 1e-9f);
        entry.crestDB = juce::jlimit(0.0f, 40.0f, entry.peakDB - entry.rmsDB);
        entry.silent = entry.rmsDB < kSilenceRmsDB;
    }
    else
    {
        entry.silent = true;
        entry.rmsDB = -100.0f;
        entry.peakDB = -100.0f;
    }

    if ((int) bars.size() <= currentBar)
    {
        while ((int) bars.size() < currentBar)
        {
            BarEntry gap;
            gap.bar = (int) bars.size();
            gap.silent = true;
            gap.rmsDB = -100.0f;
            gap.peakDB = -100.0f;
            bars.push_back(gap);
        }
        bars.push_back(entry);
    }
    else
    {
        // Re-learn: media solo tra musicali, altrimenti vince la musicale
        BarEntry& prev = bars[(size_t) currentBar];
        if (! entry.silent && ! prev.silent)
        {
            prev.rmsDB = 0.5f * (prev.rmsDB + entry.rmsDB);
            prev.peakDB = 0.5f * (prev.peakDB + entry.peakDB);
            prev.crestDB = 0.5f * (prev.crestDB + entry.crestDB);
        }
        else if (! entry.silent)
        {
            prev = entry;
        }
    }
}

int SongDynamics::nearestMusicalBar(double ppqPosition, bool usePpq, int bar) const
{
    // Richiede `lock` acquisito.
    if (bars.empty()) return -1;
    int idx = usePpq ? juce::jlimit(0, (int) bars.size() - 1, barFromPpq(ppqPosition))
                     : juce::jlimit(0, (int) bars.size() - 1, bar);
    if (! bars[(size_t) idx].silent) return idx;
    for (int d = 1; d < (int) bars.size(); ++d)
    {
        if (idx - d >= 0 && ! bars[(size_t) (idx - d)].silent) return idx - d;
        if (idx + d < (int) bars.size() && ! bars[(size_t) (idx + d)].silent) return idx + d;
    }
    return -1;
}

bool SongDynamics::getEntryForBar(int bar, BarEntry& out) const
{
    juce::ScopedLock sl(lock);
    int idx = nearestMusicalBar(0.0, false, bar);
    if (idx < 0) return false;
    out = bars[(size_t) idx];
    return true;
}

bool SongDynamics::getEntryForPpq(double ppqPosition, BarEntry& out) const
{
    juce::ScopedLock sl(lock);
    int idx = nearestMusicalBar(ppqPosition, true, 0);
    if (idx < 0) return false;
    out = bars[(size_t) idx];
    return true;
}

bool SongDynamics::getAverages(float& avgRmsDB, float& avgCrestDB) const
{
    juce::ScopedLock sl(lock);
    // Le pause non avvelenano la media: il riding segue solo musica
    double r = 0, c = 0;
    int n = 0;
    for (auto& b : bars)
        if (! b.silent) { r += b.rmsDB; c += b.crestDB; ++n; }
    if (n == 0) return false;
    avgRmsDB = (float) (r / n);
    avgCrestDB = (float) (c / n);
    return true;
}

int SongDynamics::getNumBars() const
{
    juce::ScopedLock sl(lock);
    return (int) bars.size();
}

bool SongDynamics::hasData() const
{
    juce::ScopedLock sl(lock);
    return ! bars.empty();
}

juce::String SongDynamics::getStatusText() const
{
    juce::ScopedLock sl(lock);
    if (bars.empty())
        return learning.load() ? "Song: learning... play the track" : "Song: no map - enable SONG and play";
    int musical = 0;
    for (auto& b : bars) if (! b.silent) ++musical;
    juce::String s = "Song: " + juce::String((int) bars.size()) + " bars (" + juce::String(musical) + " musical)";
    if (currentBar >= 0) s += " | bar " + juce::String(currentBar + 1);
    if (learning.load()) s += " | learning";
    return s;
}

juce::ValueTree SongDynamics::toValueTree() const
{
    juce::ScopedLock sl(lock);
    juce::ValueTree v("SongDyn");
    v.setProperty("beatsPerBar", beatsPerBar, nullptr);
    v.setProperty("numBars", (int) bars.size(), nullptr);

    juce::MemoryBlock mb;
    int nb = (int) bars.size();
    mb.append(&nb, sizeof(nb));
    for (auto& b : bars)
    {
        mb.append(&b.bar, sizeof(b.bar));
        mb.append(&b.rmsDB, sizeof(b.rmsDB));
        mb.append(&b.peakDB, sizeof(b.peakDB));
        mb.append(&b.crestDB, sizeof(b.crestDB));
        int silentFlag = b.silent ? 1 : 0;
        mb.append(&silentFlag, sizeof(silentFlag));
    }
    v.setProperty("data", mb.toBase64Encoding(), nullptr);
    return v;
}

void SongDynamics::restoreFromValueTree(const juce::ValueTree& v)
{
    if (! v.isValid() || ! v.hasType("SongDyn")) return;
    juce::ScopedLock sl(lock);
    bars.clear();

    int bpb = (int) v.getProperty("beatsPerBar", 4);
    if (bpb >= 1 && bpb <= 12) beatsPerBar = bpb;

    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding(v.getProperty("data", "").toString())) return;
    if (mb.getSize() < sizeof(int)) return;

    int pos = 0;
    auto read = [&](void* dst, size_t sz) -> bool {
        if (pos + (int) sz > (int) mb.getSize()) return false;
        std::memcpy(dst, (const char*) mb.getData() + pos, sz);
        pos += (int) sz;
        return true;
    };
    int nb = 0;
    if (! read(&nb, sizeof(nb))) return;
    nb = juce::jlimit(0, MaxBars, nb);
    // Compat: mappe senza flag silent (stride 16) vs nuove (stride 20)
    int remaining = (int) mb.getSize() - pos;
    int strideNew = (int) (sizeof(int) + 3 * sizeof(float) + sizeof(int));
    bool hasSilentFlag = nb > 0 && remaining == nb * strideNew;
    for (int i = 0; i < nb; ++i)
    {
        BarEntry e;
        if (! read(&e.bar, sizeof(e.bar))) break;
        if (! read(&e.rmsDB, sizeof(e.rmsDB))) break;
        if (! read(&e.peakDB, sizeof(e.peakDB))) break;
        if (! read(&e.crestDB, sizeof(e.crestDB))) break;
        if (hasSilentFlag)
        {
            int flag = 0;
            if (! read(&flag, sizeof(flag))) break;
            e.silent = flag != 0;
        }
        else
        {
            e.silent = false;
        }
        e.bar = juce::jlimit(0, MaxBars - 1, e.bar);
        e.rmsDB = juce::jlimit(-100.0f, 20.0f, e.rmsDB);
        e.peakDB = juce::jlimit(-100.0f, 20.0f, e.peakDB);
        e.crestDB = juce::jlimit(0.0f, 40.0f, e.crestDB);
        bars.push_back(e);
    }
    currentBar = bars.empty() ? -1 : 0;
    barSumSquares = 0.0;
    barPeak = 0.0f;
    barSampleCount = 0;
}
