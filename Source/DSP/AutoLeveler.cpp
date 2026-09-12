#include "AutoLeveler.h"

const char* AutoLeveler::className(SourceClass c)
{
    switch (c)
    {
        case SourceClass::Kick:    return "Kick";
        case SourceClass::Snare:   return "Snare";
        case SourceClass::Drums:   return "Drums";
        case SourceClass::Bass:    return "Bass";
        case SourceClass::Guitar:  return "Guitar";
        case SourceClass::Keys:    return "Keys / Piano";
        case SourceClass::Strings: return "Strings / Pad";
        case SourceClass::Vocal:   return "Vocal";
        case SourceClass::Mix:     return "Full Mix";
        default:                   return "Unknown";
    }
}

juce::StringArray AutoLeveler::classNames()
{
    return { "Auto-Detect", "Kick", "Snare", "Drums", "Bass", "Guitar",
             "Keys / Piano", "Strings / Pad", "Vocal", "Full Mix" };
}

AutoLeveler::AutoParams AutoLeveler::paramsForClass(SourceClass c)
{
    AutoParams p;
    switch (c)
    {
        case SourceClass::Kick:    p = { 2, 4.0, 2,  120, true,  4,  40, 8 }; break;
        case SourceClass::Snare:   p = { 2, 4.0, 1,  100, true,  3,  80, 8 }; break;
        case SourceClass::Drums:   p = { 3, 3.0, 5,  200, true,  6,  40, 6 }; break;
        case SourceClass::Bass:    p = { 3, 4.0, 10, 250, true,  6,  30, 7 }; break;
        case SourceClass::Guitar:  p = { 4, 3.0, 5,  200, true,  6,  80, 6 }; break;
        case SourceClass::Keys:    p = { 4, 2.5, 10, 300, true,  8,  30, 5 }; break;
        case SourceClass::Strings: p = { 5, 2.0, 25, 500, true, 10,  40, 4 }; break;
        case SourceClass::Vocal:   p = { 3, 3.0, 3,  180, true,  4,  90, 8 }; break;
        case SourceClass::Mix:     p = { 6, 2.0, 15, 300, true,  8,  30, 4 }; break;
        default:                   p = { 4, 2.5, 10, 250, true,  6,  40, 6 }; break;
    }
    return p;
}

AutoLeveler::AutoLeveler() {}

double AutoLeveler::lpCoeff(double cutoff, double sr)
{
    return 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * cutoff / sr);
}

void AutoLeveler::prepare(double sr)
{
    sampleRate = sr > 8000 ? sr : 48000.0;
    reset();
}

void AutoLeveler::reset()
{
    peakEnv = 0; peakHold = 0; rmsEnv = 1e-12; floorEnv = 1e-9; slowRMS = 1e-9;
    lp150 = lp800 = lp2000 = lp6000 = 0;
    eSub = eLowMid = eMid = ePres = eAir = 0;
    zcrCount = zcrTotal = 0;
    fastRMS = slowRMSdet = 0;
    refractory = 0; blockCounter = 0; lastSample = 0;
    onsetWrite = onsetCount = 0;
    for (auto& o : onsetBuf) o = 0;
    thrSmooth = -18; avgGR = 0; grOverTime = 0; thrInit = false; validBlocks = 0;
    learning = false; learnSamples = 0;
}

float AutoLeveler::membership(float x, float a, float b, float c, float d)
{
    if (x <= a || x >= d) return 0.0f;
    if (x >= b && x <= c) return 1.0f;
    if (x < b) return (x - a) / juce::jmax(1e-6f, b - a);
    return (d - x) / juce::jmax(1e-6f, d - c);
}

void AutoLeveler::pushBlock(const float* L, const float* R, int numSamples)
{
    if (L == nullptr || numSamples <= 0) return;
    const bool mono = (R == nullptr);

    const double c150 = lpCoeff(150, sampleRate), c800 = lpCoeff(800, sampleRate);
    const double c2k = lpCoeff(2000, sampleRate), c6k = lpCoeff(6000, sampleRate);
    const double peakA = 1.0 - std::exp(-1.0 / (0.001 * sampleRate));   // ~1 ms
    const double peakR = 1.0 - std::exp(-1.0 / (0.200 * sampleRate));   // 200 ms
    const double rmsT  = 1.0 - std::exp(-1.0 / (0.400 * sampleRate));   // 400 ms
    const double slowT = 1.0 - std::exp(-1.0 / (1.200 * sampleRate));   // 1.2 s
    const double fFast = 1.0 - std::exp(-1.0 / (0.010 * sampleRate));   // onset fast
    const double fSlow = 1.0 - std::exp(-1.0 / (0.300 * sampleRate));   // onset slow

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = L[i];
        const float r = mono ? l : R[i];
        const double m = 0.5 * (l + r);
        const double a = std::abs(m);

        // Envelopes
        const double pc = (a > peakEnv) ? peakA : peakR;
        peakEnv += pc * (a - peakEnv);
        // Peak-hold: catches transients, releases over ~1.5 s so crest and
        // dynamic range stay meaningful between hits (not snapshot-phase noise)
        const double holdR = 1.0 - std::exp(-1.0 / (1.500 * sampleRate));
        peakHold += ((a > peakHold) ? peakA : holdR) * (a - peakHold);
        rmsEnv += rmsT * (m * m - rmsEnv);
        slowRMS += slowT * (m * m - slowRMS);
        // Floor: true slow-minimum tracker. Falls toward quiet in ~50 ms,
        // climbs back only over ~2 s, so dynRange = peakHold - floor reflects
        // the played dynamics (hits vs valleys), not the noise floor of time zero.
        const double downC = 1.0 - std::exp(-1.0 / (0.050 * sampleRate));
        const double upC = 1.0 - std::exp(-1.0 / (2.000 * sampleRate));
        floorEnv += ((a < floorEnv) ? downC : upC) * (a - floorEnv);

        // 5-band energies (one-pole LP ladder on squared signal).
        // Differences are clamped >= 0: ladder stages have different group
        // delays, so raw differences can momentarily flip sign.
        const double ms = m * m;
        lp150  += c150 * (ms - lp150);
        lp800  += c800 * (ms - lp800);
        lp2000 += c2k  * (ms - lp2000);
        lp6000 += c6k  * (ms - lp6000);
        const double eT = 0.02;
        eSub    += eT * (juce::jmax(0.0, lp150) - eSub);
        eLowMid += eT * (juce::jmax(0.0, lp800 - lp150) - eLowMid);
        eMid    += eT * (juce::jmax(0.0, lp2000 - lp800) - eMid);
        ePres   += eT * (juce::jmax(0.0, lp6000 - lp2000) - ePres);
        eAir    += eT * (juce::jmax(0.0, ms - lp6000) - eAir);

        // ZCR (gated at -40 dBFS)
        if (a > 0.01)
        {
            zcrTotal += 1.0;
            if ((lastSample <= 0 && m > 0) || (lastSample >= 0 && m < 0)) zcrCount += 1.0;
        }
        lastSample = (float)m;

        // Onset: fast vs slow detector
        fastRMS += fFast * (ms - fastRMS);
        slowRMSdet += fSlow * (ms - slowRMSdet);
        if (refractory > 0) --refractory;
        else if (fastRMS > slowRMSdet * 3.2 && fastRMS > 1e-6) // ~5 dB jump
        {
            // coarse flag slot
            refractory = (int)(sampleRate * 0.05); // 50 ms
            int slot = onsetWrite % (int)(sizeof(onsetBuf) / sizeof(onsetBuf[0]));
            if (onsetBuf[slot] == 0) { onsetBuf[slot] = 1; ++onsetCount; }
        }

        if (learning) ++learnSamples;
    }

    // Advance coarse onset window every 64 samples worth of blocks
    blockCounter += numSamples;
    while (blockCounter >= 64)
    {
        blockCounter -= 64;
        int slot = onsetWrite % (int)(sizeof(onsetBuf) / sizeof(onsetBuf[0]));
        onsetWrite++;
        // expire flags older than 4 s window
        int windowSlots = (int)(sampleRate * 4.0 / 64.0);
        int oldSlot = (onsetWrite - windowSlots) % (int)(sizeof(onsetBuf) / sizeof(onsetBuf[0]));
        if (oldSlot < 0) oldSlot += (int)(sizeof(onsetBuf) / sizeof(onsetBuf[0]));
        if (onsetBuf[oldSlot] != 0) { onsetBuf[oldSlot] = 0; --onsetCount; }
        if (onsetCount < 0) onsetCount = 0;
        (void)slot;
    }
}

AutoLeveler::Features AutoLeveler::getFeatures() const
{
    // Note: members are written on the audio thread (lock-free by design,
    // aligned doubles read atomically in practice on x86-64/ARM64).
    Features f;
    const double rms = std::sqrt(juce::jmax(rmsEnv, 1e-12));
    const double slow = std::sqrt(juce::jmax(slowRMS, 1e-12));
    f.rmsDB = juce::Decibels::gainToDecibels(rms + 1e-9);
    f.peakDB = juce::Decibels::gainToDecibels(peakHold + 1e-9);
    f.crestDB = f.peakDB - juce::Decibels::gainToDecibels(slow + 1e-9);
    f.dynRangeDB = juce::jlimit(0.0, 40.0, f.peakDB - juce::Decibels::gainToDecibels(floorEnv + 1e-9));
    f.valid = f.rmsDB > -60.0;

    const double tot = eSub + eLowMid + eMid + ePres + eAir + 1e-12;
    f.subRatio = eSub / tot;
    f.presenceRatio = ePres / tot;
    f.airRatio = eAir / tot;
    const double shares[5] = { eSub / tot, eLowMid / tot, eMid / tot, ePres / tot, eAir / tot };
    double mx = 0;
    for (double s : shares) mx = juce::jmax(mx, s);
    f.spectralSpread = mx / 0.2; // 1 = perfectly flat, 5 = all in one band
    const double cent = 75 * eSub + 400 * eLowMid + 1400 * eMid + 4000 * ePres + 10000 * eAir;
    f.centroidHz = cent / tot;
    f.zcr = zcrTotal > 100 ? juce::jlimit(0.0, 1.0, zcrCount / zcrTotal * 0.5) : 0.0;
    // Onset window scales with sample rate (buffer holds 3000 coarse slots)
    constexpr double bufSlots = double(sizeof(onsetBuf) / sizeof(onsetBuf[0]));
    const double windowSec = juce::jmin(4.0, bufSlots * 64.0 / sampleRate);
    f.transientRate = onsetCount / juce::jmax(0.5, windowSec);
    (void)slow;
    return f;
}

AutoLeveler::ClassResult AutoLeveler::classify(const Features& f, SourceClass prior) const
{
    ClassResult r;
    if (!f.valid)
    {
        r.cls = SourceClass::Unknown;
        r.scores[(int)SourceClass::Unknown] = 1.0f;
        return r;
    }
    float* s = r.scores;
    const float tr = (float)f.transientRate;
    const float crest = (float)f.crestDB;
    const float dyn = (float)f.dynRangeDB;
    const float cent = (float)f.centroidHz;
    const float sub = (float)f.subRatio;
    const float pres = (float)f.presenceRatio;
    const float air = (float)f.airRatio;

    // Heuristic rule set: each class accumulates evidence 0..~3
    s[(int)SourceClass::Kick] =
          1.2f * membership(sub, 0.30f, 0.45f, 1.0f, 2.0f)
        + 1.0f * membership(tr, 1.0f, 2.0f, 30.0f, 60.0f)
        + 0.8f * membership(crest, 8.0f, 11.0f, 30.0f, 60.0f)
        + 0.8f * membership(sub, 0.60f, 0.75f, 1.0f, 2.0f)
        + 0.4f * membership(cent, 0.0f, 0.0f, 250.0f, 500.0f)
        - 1.0f * membership(pres + air, 0.12f, 0.20f, 1.0f, 2.0f); // veto: wires = snare

    s[(int)SourceClass::Snare] =
          1.1f * membership(pres, 0.05f, 0.08f, 1.0f, 2.0f)
        + 0.9f * membership(tr, 1.0f, 1.5f, 30.0f, 60.0f)
        + 0.6f * membership(cent, 800.0f, 1500.0f, 6000.0f, 9000.0f)
        + 0.4f * (1.0f - membership(sub, 0.30f, 0.45f, 1.0f, 2.0f))
        - 0.8f * membership(sub, 0.55f, 0.70f, 1.0f, 2.0f); // veto: too subby = kick

    s[(int)SourceClass::Drums] =
          1.0f * membership(tr, 2.0f, 4.0f, 40.0f, 80.0f)
        + 0.7f * membership(crest, 7.0f, 9.0f, 30.0f, 60.0f)
        + 0.4f * membership(dyn, 8.0f, 12.0f, 40.0f, 80.0f);

    s[(int)SourceClass::Bass] =
          1.2f * membership(cent, 0.0f, 0.0f, 400.0f, 800.0f)
        + 0.7f * membership(sub, 0.35f, 0.5f, 1.0f, 2.0f)
        + 0.6f * (1.0f - membership(tr, 1.0f, 2.0f, 40.0f, 80.0f))
        + 0.4f * membership(crest, 2.0f, 3.0f, 10.0f, 14.0f);

    s[(int)SourceClass::Guitar] =
          0.9f * membership(cent, 300.0f, 500.0f, 2000.0f, 3000.0f)
        + 0.7f * membership(tr, 0.8f, 1.5f, 6.0f, 10.0f)
        + 0.6f * membership(crest, 5.0f, 7.0f, 13.0f, 17.0f)
        + 0.4f * membership(pres, 0.08f, 0.12f, 0.4f, 0.6f);

    s[(int)SourceClass::Keys] =
          0.8f * membership(crest, 7.0f, 9.0f, 15.0f, 19.0f)
        + 0.7f * membership(tr, 0.3f, 0.8f, 4.0f, 7.0f)
        + 0.5f * membership(cent, 300.0f, 600.0f, 3500.0f, 5000.0f)
        + 0.4f * membership(dyn, 8.0f, 12.0f, 30.0f, 50.0f);

    s[(int)SourceClass::Strings] =
          1.0f * (1.0f - membership(tr, 0.5f, 1.5f, 20.0f, 40.0f))
        + 0.8f * (1.0f - membership(crest, 8.0f, 10.0f, 30.0f, 60.0f))
        + 0.5f * membership(cent, 250.0f, 400.0f, 3500.0f, 5000.0f)
        + 0.4f * membership(air, 0.05f, 0.10f, 1.0f, 2.0f);

    s[(int)SourceClass::Vocal] =
          1.1f * membership(pres, 0.08f, 0.12f, 0.60f, 0.90f)
        + 0.8f * membership(crest, 6.0f, 8.0f, 14.0f, 18.0f)
        + 0.7f * membership(dyn, 7.0f, 10.0f, 30.0f, 50.0f)
        + 0.5f * membership(cent, 400.0f, 800.0f, 3500.0f, 5000.0f)
        + 0.4f * (1.0f - membership(tr, 3.0f, 5.0f, 40.0f, 80.0f));

    s[(int)SourceClass::Mix] =
          1.0f * (1.0f - membership(crest, 8.0f, 10.0f, 30.0f, 60.0f))
        + 0.8f * (1.0f - membership(tr, 1.0f, 2.0f, 20.0f, 40.0f))
        + 0.5f * (1.0f - membership(dyn, 10.0f, 14.0f, 40.0f, 80.0f))
        + 0.5f * membership(cent, 400.0f, 700.0f, 3000.0f, 5000.0f)
        + 0.6f * (1.0f - membership((float)f.spectralSpread, 1.0f, 1.2f, 2.0f, 2.6f))
        - 0.9f * membership(sub, 0.50f, 0.65f, 1.0f, 2.0f); // veto: sub monster = bass/kick

    // User profile acts as a prior: gentle boost, never a hard override
    if (prior != SourceClass::Unknown && (int)prior >= 0 && (int)prior < (int)SourceClass::NumClasses)
        s[(int)prior] *= 1.6f;

    // Winner + confidence from margin
    int best = (int)SourceClass::Mix, second = best;
    for (int i = 0; i < (int)SourceClass::NumClasses; ++i)
    {
        if (i == (int)SourceClass::Unknown) continue;
        if (s[i] > s[best]) { second = best; best = i; }
        else if (i != best && s[i] > s[second]) second = i;
    }
    r.cls = (SourceClass)best;
    const float margin = s[best] - s[second];
    r.confidence = juce::jlimit(0.0f, 1.0f, margin / juce::jmax(0.5f, s[best]));
    return r;
}

void AutoLeveler::noteGainReduction(float grDB)
{
    avgGR += 0.02 * (grDB - avgGR); // ~block-rate smoothing
}

double AutoLeveler::updateAuto(double currentThresholdDB, int numSamples,
                               SourceClass lockedProfile, double& outAvgGR)
{
    Features f = getFeatures();
    outAvgGR = avgGR;
    if (!f.valid) { validBlocks = 0; return currentThresholdDB; }
    if (validBlocks < 1000000) ++validBlocks;

    // Target class: locked profile or live classifier
    SourceClass cls = lockedProfile;
    if (cls == SourceClass::Unknown)
    {
        ClassResult r = classify(f);
        cls = (r.confidence > 0.15f) ? r.cls : SourceClass::Mix;
    }
    AutoParams ap = paramsForClass(cls);

    if (!thrInit) { thrSmooth = currentThresholdDB; thrInit = true; }

    // GR-cap servo: if we constantly exceed the class ceiling, ride the threshold up
    if (avgGR > ap.maxGRdB + 1.0) grOverTime += 1.0;
    else if (grOverTime > 0) grOverTime -= 0.2;
    double servo = 0.0;
    if (grOverTime > 120.0) { servo = (grOverTime - 120.0) * 0.005; grOverTime = 120.0 + (grOverTime - 120.0) * 0.999; }
    servo = juce::jlimit(0.0, 6.0, servo);

    const double target = f.rmsDB + ap.thrOffsetDB + servo;
    const double gap = target - thrSmooth;

    // Per-block coefficients from block duration (transport-start safe).
    // Large gaps (fresh start, arrangement jump) converge in ~0.2 s;
    // fine riding stays gentle: down fast, up slow (no pumping).
    const double blkSec = juce::jmax(1, numSamples) / sampleRate;
    const double snapC = 1.0 - std::exp(-blkSec / 0.080); // priming: huge gap
    const double fastC = 1.0 - std::exp(-blkSec / 0.150); // follow music
    const double slowC = 1.0 - std::exp(-blkSec / 1.500); // fine riding
    const double ag = std::abs(gap);
    // Priming window: during the first ~1 s of valid audio (e.g. transport
    // start) always converge fast, so playback never swells in slowly.
    const double primeBlocks = sampleRate / juce::jmax(1, numSamples);
    double c;
    if (validBlocks < primeBlocks) c = snapC;
    else if (ag > 8.0) c = snapC;
    else if (ag > 3.0) c = fastC;
    else c = (gap < 0.0) ? fastC : slowC;
    thrSmooth += c * gap;
    return juce::jlimit(-60.0, 0.0, thrSmooth);
}

void AutoLeveler::beginLearn()
{
    learning = true;
    learnSamples = 0;
}

bool AutoLeveler::learnReady() const
{
    return learning && learnSamples >= kLearnSamples;
}

float AutoLeveler::learnProgress() const
{
    if (!learning) return 0.0f;
    return juce::jlimit(0.0f, 1.0f, (float)learnSamples / (float)kLearnSamples);
}
