#include "Compressor.h"

Compressor::Compressor() {}

void Compressor::prepare(double sr)
{
    sampleRate = sr > 8000 ? sr : 48000.0;
    reset();
    updateSidechainFilter();
}

float Compressor::saturate(float x, float driveGain, int model)
{
    if (driveGain <= 1.001f || model == CircuitModelInfo::Modern) return x; // clean
    const float u = x * driveGain;
    float y = u;
    switch (model)
    {
        case CircuitModelInfo::VariMu:
        case CircuitModelInfo::Opto:
        {
            // Tube: asymmetric soft clip (2nd-harmonic warmth)
            y = std::tanh(u + 0.18f * u * std::abs(u));
            y /= std::tanh(driveGain + 0.18f * driveGain * driveGain);
            break;
        }
        case CircuitModelInfo::FET:
        {
            // FET: harder atan edge (aggressive grit)
            y = std::atan(1.6f * u) / std::atan(1.6f * driveGain);
            break;
        }
        default: // VCA, Diode: transformer-style symmetric soft saturation
        {
            y = std::tanh(u) / std::tanh(driveGain);
            break;
        }
    }
    if (!std::isfinite(y)) return x;
    return y;
}

void Compressor::reset()
{
    peakEnv = rmsAvg = rmsSm = grSmooth = grSlow = mkSmooth = 0.0f;
    scL = HpState(); scR = HpState();
    grMeter.store(0.0f);
    inMeter.store(-100.0f);
    outMeter.store(-100.0f);
}

float Compressor::msToCoeff(float ms) const
{
    if (ms <= 0.01f) ms = 0.01f;
    // one-pole: coeff = 1 - exp(-1 / (ms/1000 * sr))
    return 1.0f - std::exp(-1.0f / (float)(ms * 0.001 * sampleRate));
}

void Compressor::updateSidechainFilter()
{
    if (params.scHPF <= 20.0)
    {
        scB0 = 1; scB1 = scB2 = scA1 = scA2 = 0; // bypass
        return;
    }
    double f = juce::jlimit(20.0, sampleRate * 0.45, params.scHPF);
    const double Q = 0.707;
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
    const double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * Q);
    const double a0 = 1 + alpha;
    scB0 = float(((1 + cosw0) / 2) / a0);
    scB1 = float((-(1 + cosw0)) / a0);
    scB2 = float(((1 + cosw0) / 2) / a0);
    scA1 = float((-2 * cosw0) / a0);
    scA2 = float((1 - alpha) / a0);
}

void Compressor::setParams(const CompParams& p)
{
    bool scChanged = (p.scHPF != params.scHPF);
    params = p;
    // Clamp everything to sane ranges (host automation safety)
    params.thresholdDB = juce::jlimit(-60.0, 0.0, params.thresholdDB);
    params.ratio       = juce::jlimit(1.0, 20.0, params.ratio);
    params.attackMs    = juce::jlimit(0.02, 300.0, params.attackMs); // 0.02 ms for FET-style snap
    params.releaseMs   = juce::jlimit(20.0, 3000.0, params.releaseMs);
    params.kneeDB      = juce::jlimit(0.0, 12.0, params.kneeDB);
    params.makeupDB    = juce::jlimit(0.0, 24.0, params.makeupDB);
    params.mix         = juce::jlimit(0.0, 1.0, params.mix);
    params.scHPF       = juce::jlimit(10.0, 20000.0, params.scHPF);
    params.detectorMix = juce::jlimit(0.0, 1.0, params.detectorMix);
    params.ceilingDB   = juce::jlimit(-12.0, 0.0, params.ceilingDB);
    params.circuitModel = juce::jlimit(0, (int)CircuitModelInfo::NumModels - 1, params.circuitModel);
    params.driveDB     = juce::jlimit(0.0, 24.0, params.driveDB);
    if (scChanged) { scL = HpState(); scR = HpState(); updateSidechainFilter(); }
}

float Compressor::staticGainReduction(float levelDB, float thrDB, float ratio, float kneeDB)
{
    const float over = levelDB - thrDB;
    const float knee = juce::jmax(0.001f, kneeDB);
    const bool inf = ratio >= 19.5f;
    if (2.0f * over <= -knee) return 0.0f;
    if (2.0f * over >= knee || kneeDB <= 0.001f)
        return inf ? over : over * (1.0f - 1.0f / ratio); // positive reduction dB
    // Soft-knee region: quadratic blend, C1 continuous
    const float x = over + knee * 0.5f;
    const float slope = inf ? 1.0f : (1.0f - 1.0f / ratio);
    return slope * x * x / (2.0f * knee);
}

void Compressor::processBlock(juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    const float atkC = msToCoeff((float)(params.limiter ? juce::jmin(params.attackMs, 1.0) : params.attackMs));
    const float relBaseC = msToCoeff((float)params.releaseMs);
    const float optoTailC = msToCoeff(1500.0f); // LA-2A-style slow tail
    // Auto-makeup ballistics: follow GR *up* fast (no fade-in dip on transport
    // start), let go slowly (no pumping when GR releases) - classic behavior.
    const float mkAtkC = msToCoeff(3.0f);
    const float mkRelC = msToCoeff(600.0f);
    rmsAvgC = msToCoeff(10.0f); // RMS pre-average window: fixed 10 ms
    const float peakBlend = float(1.0 - params.detectorMix);
    const float rmsBlend  = float(params.detectorMix);
    const float thr = (float)params.thresholdDB;
    const float knee = (float)params.kneeDB;
    const float baseRatio = (float)(params.limiter ? 20.0 : params.ratio);
    const float ceilLin = juce::Decibels::decibelsToGain((float)params.ceilingDB);
    const bool isVariMu = (params.circuitModel == CircuitModelInfo::VariMu && !params.limiter);
    const bool isOpto = (params.circuitModel == CircuitModelInfo::Opto && !params.limiter);
    const float driveGain = juce::Decibels::decibelsToGain(
        juce::jmax(CircuitModelInfo::driveFloor(params.circuitModel), (float)params.driveDB));

    const bool mono = (numCh == 1);
    float* L = buffer.getWritePointer(0);
    float* R = mono ? nullptr : buffer.getWritePointer(1);

    float blockMaxGR = 0.0f;
    float blockPeakIn = 0.0f, blockPeakOut = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float xl = L[i];
        const float xr = mono ? xl : R[i];
        const float axl = std::abs(xl), axr = std::abs(xr);
        blockPeakIn = juce::jmax(blockPeakIn, juce::jmax(axl, axr));

        // --- Linked sidechain: HPF the audio first, then rectify + max ---
        const float fl = scFilter(xl, scL);
        const float fr = mono ? fl : scFilter(xr, scR);
        const float sc = juce::jmax(std::abs(fl), std::abs(fr));

        // Peak envelope (switched attack/release on linear signal)
        const float pC = (sc > peakEnv) ? atkC : relBaseC;
        peakEnv += pC * (sc - peakEnv);
        // RMS path: fixed short-term average FIRST (true mean square, unbiased),
        // then the same attack/release ballistics on the smoothed level.
        const float ms = sc * sc;
        rmsAvg += rmsAvgC * (ms - rmsAvg);
        const float rmsLvl = std::sqrt(juce::jmax(rmsAvg, 1e-12f));
        const float qC = (rmsLvl > rmsSm) ? atkC : relBaseC;
        rmsSm += qC * (rmsLvl - rmsSm);
        const float peakLin = peakEnv;
        const float rmsLin = rmsSm;
        const float detLin = peakLin * peakBlend + rmsLin * rmsBlend;
        const float levelDB = juce::Decibels::gainToDecibels(detLin + 1e-9f);

        // --- Gain computer (positive reduction dB) ---
        // Vari-Mu progressive ratio: the deeper the GR, the higher the ratio
        // (Fairchild-style program dependence), capped at brickwall.
        float ratioEff = baseRatio;
        if (isVariMu) ratioEff = juce::jmin(20.0f, baseRatio * (1.0f + 0.06f * grSmooth));
        const float target = staticGainReduction(levelDB, thr, ratioEff, knee);

        // --- Opto slow tail: second very-slow follower, GR takes the max ---
        float smoothTarget = target;
        if (isOpto)
        {
            const float sC = (target > grSlow) ? atkC : optoTailC;
            grSlow += sC * (target - grSlow);
            smoothTarget = juce::jmax(target, grSlow * 0.995f);
        }
        else grSlow = target; // keep the tail follower parked when unused

        // --- GR smoothing with program-dependent auto release ---
        float relC = relBaseC;
        if (params.autoRelease)
        {
            // Deeper/longer reduction -> slower release (SSL-auto style glue),
            // capped x5; transients still punch through via fast attack.
            const float depth = juce::jlimit(0.0f, 12.0f, grSmooth);
            const float effMs = float(params.releaseMs * (1.0 + depth * 0.33));
            relC = msToCoeff(juce::jmin(effMs, 3000.0f));
        }
        const float gC = (smoothTarget > grSmooth) ? atkC : relC;
        grSmooth += gC * (smoothTarget - grSmooth);
        if (std::abs(grSmooth) < 1e-6f) grSmooth = 0.0f;
        blockMaxGR = juce::jmax(blockMaxGR, grSmooth);

        // --- Auto makeup follows smoothed GR so loudness stays matched ---
        const float mC = (grSmooth > mkSmooth) ? mkAtkC : mkRelC;
        mkSmooth += mC * (grSmooth - mkSmooth);
        const float makeup = params.autoMakeup ? juce::jlimit(0.0f, 18.0f, mkSmooth)
                                               : (float)params.makeupDB;
        const float wetGain = juce::Decibels::decibelsToGain(-grSmooth + makeup);
        // Vintage saturation on the wet path only (parallel dry stays clean)
        float wetL = saturate(xl * wetGain, driveGain, params.circuitModel);
        float wetR = mono ? wetL : saturate(xr * wetGain, driveGain, params.circuitModel);
        const float m = (float)params.mix;
        float yl = xl + (wetL - xl) * m;
        float yr = mono ? yl : xr + (wetR - xr) * m;

        // --- Limiter ceiling (brickwall) ---
        if (params.limiter)
        {
            yl = juce::jlimit(-ceilLin, ceilLin, yl);
            if (!mono) yr = juce::jlimit(-ceilLin, ceilLin, yr);
        }

        // Sanitize + denormal guard
        if (!std::isfinite(yl)) yl = 0.0f;
        if (std::abs(yl) < 1e-20f) yl = 0.0f;
        L[i] = yl;
        blockPeakOut = juce::jmax(blockPeakOut, std::abs(yl));
        if (!mono)
        {
            if (!std::isfinite(yr)) yr = 0.0f;
            if (std::abs(yr) < 1e-20f) yr = 0.0f;
            R[i] = yr;
            blockPeakOut = juce::jmax(blockPeakOut, std::abs(yr));
        }

        // Extra channels: copy linked wet gain (+ saturation like L/R)
        for (int ch = 2; ch < numCh; ++ch)
        {
            float x = buffer.getWritePointer(ch)[i];
            float w = saturate(x * wetGain, driveGain, params.circuitModel);
            float y = x + (w - x) * m;
            if (params.limiter) y = juce::jlimit(-ceilLin, ceilLin, y);
            if (!std::isfinite(y) || std::abs(y) < 1e-20f) y = 0.0f;
            buffer.getWritePointer(ch)[i] = y;
        }
    }

    grMeter.store(blockMaxGR);
    inMeter.store(juce::Decibels::gainToDecibels(blockPeakIn + 1e-9f));
    outMeter.store(juce::Decibels::gainToDecibels(blockPeakOut + 1e-9f));
}
