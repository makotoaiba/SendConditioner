#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String hzText (float v)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    }

    juce::NormalisableRange<float> freqRange (float lo, float hi)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;

    std::unique_ptr<APF> makeFloat (const char* id, const char* name, juce::NormalisableRange<float> range,
                                    float def, std::function<juce::String (float)> toText)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def,
                                      Attr().withStringFromValueFunction ([toText] (float v, int) { return toText (v); }));
    }

    juce::String dbText (float v)  { return juce::String (v, 1) + " dB"; }
    juce::String msText (float v)  { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String pctText (float v) { return juce::String (juce::roundToInt (v)) + " %"; }

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (juce::jmax (g, 1.0e-6f)); }
}

juce::AudioProcessorValueTreeState::ParameterLayout SendConditionerProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Pre-delay
    l.add (std::make_unique<APB> (juce::ParameterID { scp::pdSync, 1 }, "PreDelay Sync", true));
    l.add (std::make_unique<APC> (juce::ParameterID { scp::pdNote, 1 }, "PreDelay Note",
                                  scp::predelayNotes().names, 3));
    {
        juce::NormalisableRange<float> r (0.0f, 500.0f); r.setSkewForCentre (60.0f);
        l.add (makeFloat (scp::pdMs, "PreDelay ms", r, 0.0f, msText));
    }

    // Filter / width
    l.add (makeFloat (scp::hpf, "HPF", freqRange (20.0f, 2000.0f), 150.0f,
                      [] (float v) { return v <= 20.5f ? juce::String ("Off") : hzText (v); }));
    l.add (makeFloat (scp::lpf, "LPF", freqRange (1000.0f, 20000.0f), 9000.0f,
                      [] (float v) { return v >= 19999.0f ? juce::String ("Off") : hzText (v); }));
    l.add (std::make_unique<APC> (juce::ParameterID { scp::slope, 1 }, "Filter Slope",
                                  juce::StringArray { "12 dB/oct", "24 dB/oct" }, 0));
    l.add (makeFloat (scp::monoLow, "Mono Below", freqRange (20.0f, 500.0f), 20.0f,
                      [] (float v) { return v <= 20.5f ? juce::String ("Off") : hzText (v); }));
    l.add (makeFloat (scp::width, "Width", { 0.0f, 200.0f }, 100.0f, pctText));

    // Sidechain ducker
    l.add (std::make_unique<APB> (juce::ParameterID { scp::duckOn, 1 }, "Duck On", false));
    l.add (makeFloat (scp::duckThr, "Duck Threshold", { -60.0f, 0.0f }, -30.0f, dbText));
    l.add (makeFloat (scp::duckAmt, "Duck Depth", { 0.0f, 40.0f }, 10.0f,
                      [] (float v) { return "-" + juce::String (v, 1) + " dB"; }));
    {
        juce::NormalisableRange<float> a (1.0f, 200.0f);   a.setSkewForCentre (20.0f);
        juce::NormalisableRange<float> r (20.0f, 2000.0f); r.setSkewForCentre (250.0f);
        l.add (makeFloat (scp::duckAtk, "Duck Attack", a, 10.0f, msText));
        l.add (makeFloat (scp::duckRel, "Duck Release", r, 300.0f, msText));
    }

    // Tail gate
    l.add (std::make_unique<APB> (juce::ParameterID { scp::gateOn, 1 }, "Gate On", false));
    l.add (makeFloat (scp::gateThr, "Gate Threshold", { -80.0f, -10.0f }, -45.0f, dbText));
    l.add (std::make_unique<APC> (juce::ParameterID { scp::gateLen, 1 }, "Gate Hold",
                                  scp::gateLengths().names, 1));
    {
        juce::NormalisableRange<float> r (5.0f, 500.0f); r.setSkewForCentre (60.0f);
        l.add (makeFloat (scp::gateRel, "Gate Release", r, 40.0f, msText));
    }

    // Tempo LFO
    l.add (std::make_unique<APB> (juce::ParameterID { scp::lfoOn, 1 }, "LFO On", false));
    l.add (std::make_unique<APC> (juce::ParameterID { scp::lfoRate, 1 }, "LFO Rate",
                                  scp::lfoRates().names, 5));
    l.add (std::make_unique<APC> (juce::ParameterID { scp::lfoShape, 1 }, "LFO Shape",
                                  juce::StringArray { "Pump", "Sine", "Ramp" }, 0));
    l.add (makeFloat (scp::lfoAmt, "LFO Depth", { 0.0f, 24.0f }, 6.0f,
                      [] (float v) { return "-" + juce::String (v, 1) + " dB"; }));

    // Output
    l.add (std::make_unique<APB> (juce::ParameterID { scp::autoGain, 1 }, "Auto Gain", true));
    l.add (makeFloat (scp::mix, "Mix", { 0.0f, 100.0f }, 100.0f, pctText));
    l.add (makeFloat (scp::outGain, "Output", { -24.0f, 12.0f }, 0.0f, dbText));
    l.add (std::make_unique<APB> (juce::ParameterID { scp::monoCheck, 1 }, "Mono Check", false));
    l.add (std::make_unique<APB> (juce::ParameterID { scp::compare, 1 }, "Compare (Bypass)", false));

    return l;
}

SendConditionerProcessor::SendConditionerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SendConditioner", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    p = { g (scp::pdSync), g (scp::pdNote), g (scp::pdMs),
          g (scp::hpf), g (scp::lpf), g (scp::slope), g (scp::monoLow), g (scp::width),
          g (scp::duckOn), g (scp::duckThr), g (scp::duckAmt), g (scp::duckAtk), g (scp::duckRel),
          g (scp::gateOn), g (scp::gateThr), g (scp::gateLen), g (scp::gateRel),
          g (scp::lfoOn), g (scp::lfoRate), g (scp::lfoShape), g (scp::lfoAmt),
          g (scp::autoGain), g (scp::mix), g (scp::outGain), g (scp::monoCheck), g (scp::compare) };
}

bool SendConditionerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != mono && out != stereo) return false;
    if (layouts.getMainInputChannelSet() != out) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto scSet = layouts.getChannelSet (true, 1);
        if (! scSet.isDisabled() && scSet != mono && scSet != stereo) return false;
    }
    return true;
}

void SendConditionerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    delay.setMaximumDelayInSamples ((int) std::ceil (maxPredelaySeconds * sampleRate) + 8);
    delay.prepare (spec);
    delay.reset();

    for (auto* f : { &hp1, &hp2 }) { f->prepare (spec); f->setType (juce::dsp::StateVariableTPTFilterType::highpass); }
    for (auto* f : { &lp1, &lp2 }) { f->prepare (spec); f->setType (juce::dsp::StateVariableTPTFilterType::lowpass); }
    crossover.prepare (spec);
    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    delaySamples.reset (sampleRate, 0.08);
    mixSm.reset (sampleRate, 0.03);
    outSm.reset (sampleRate, 0.03);
    widthSm.reset (sampleRate, 0.03);
    compareSm.reset (sampleRate, 0.02);

    delaySamples.setCurrentAndTargetValue (0.0f);
    mixSm.setCurrentAndTargetValue (param<float> (p.mix) * 0.01f);
    outSm.setCurrentAndTargetValue (dbToGain (param<float> (p.outGain)));
    widthSm.setCurrentAndTargetValue (param<float> (p.width) * 0.01f);
    compareSm.setCurrentAndTargetValue (isOn (p.compare) ? 1.0f : 0.0f);

    scEnv = inEnv = 0.0f;
    duckDb = 0.0f; gateGain = 1.0f; autoGainLin = 1.0f;
    preMs = postMs = 0.0f;
    gateHold = 0;
    scSilentSamples = 1 << 30;
    corrSm = 1.0f;
}

float SendConditionerProcessor::coeffForMs (float ms) const
{
    return std::exp (-1.0f / (juce::jmax (0.01f, ms) * 0.001f * (float) sr));
}

void SendConditionerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto io = getBusBuffer (buffer, false, 0);
    const int n = io.getNumSamples();
    const int numCh = io.getNumChannels();
    if (n == 0 || numCh == 0) return;

    // clear any extra output channels (none with our layouts, but be safe)
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    const bool stereo = numCh > 1;
    float* L = io.getWritePointer (0);
    float* R = stereo ? io.getWritePointer (1) : nullptr;

    // ---- sidechain
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            if (scBuf.getNumChannels() > 0)
            {
                scL = scBuf.getReadPointer (0);
                scR = scBuf.getNumChannels() > 1 ? scBuf.getReadPointer (1) : scL;
            }
        }
    const bool scBus = scL != nullptr;

    // ---- tempo
    double bpm = 120.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const double ppqPerSample = bpm / 60.0 / sr;
    const double secPerBeat = 60.0 / bpm;

    // ---- block parameters
    const double pdSec = isOn (p.pdSync)
                           ? scp::beatsAt (scp::predelayNotes(), param<int> (p.pdNote)) * secPerBeat
                           : param<float> (p.pdMs) * 0.001;
    delaySamples.setTargetValue ((float) (juce::jlimit (0.0, maxPredelaySeconds, pdSec) * sr));

    const float nyq = (float) sr * 0.45f;
    const float hpHz = param<float> (p.hpf), lpHz = param<float> (p.lpf);
    const bool hpOn = hpHz > 20.5f, lpOn = lpHz < 19999.0f, steep = param<int> (p.slope) == 1;
    for (auto* f : { &hp1, &hp2 }) f->setCutoffFrequency (juce::jmin (hpHz, nyq));
    for (auto* f : { &lp1, &lp2 }) f->setCutoffFrequency (juce::jmin (lpHz, nyq));
    // Q for 12 dB = Butterworth; cascaded pair approximates a 24 dB Butterworth response
    const float q1 = steep ? 0.5412f : 0.7071f, q2 = 1.3066f;
    hp1.setResonance (q1); lp1.setResonance (q1); hp2.setResonance (q2); lp2.setResonance (q2);

    const float monoHz = param<float> (p.monoLow);
    const bool monoOn = stereo && monoHz > 20.5f;
    crossover.setCutoffFrequency (juce::jmin (monoHz, nyq));

    widthSm.setTargetValue (param<float> (p.width) * 0.01f);
    mixSm.setTargetValue (param<float> (p.mix) * 0.01f);
    outSm.setTargetValue (dbToGain (param<float> (p.outGain)));
    compareSm.setTargetValue (isOn (p.compare) ? 1.0f : 0.0f);

    const bool duckOn = isOn (p.duckOn);
    const float duckThr = param<float> (p.duckThr), duckAmt = param<float> (p.duckAmt);
    const float duckAtkC = coeffForMs (param<float> (p.duckAtk));
    const float duckRelC = coeffForMs (param<float> (p.duckRel));

    const bool gateOn = isOn (p.gateOn);
    const float gateThr = param<float> (p.gateThr);
    const int gateHoldLen = (int) (scp::beatsAt (scp::gateLengths(), param<int> (p.gateLen)) * secPerBeat * sr);
    const float gateOpenC = coeffForMs (1.0f), gateCloseC = coeffForMs (param<float> (p.gateRel));

    const bool lfoOn = isOn (p.lfoOn);
    const double lfoBeats = scp::beatsAt (scp::lfoRates(), param<int> (p.lfoRate));
    const int lfoShape = param<int> (p.lfoShape);
    const float lfoAmt = param<float> (p.lfoAmt);

    const bool autoGainOn = isOn (p.autoGain);
    const bool monoCheck = isOn (p.monoCheck);

    const float envAtk = coeffForMs (0.5f), envRel = coeffForMs (60.0f);
    const float msC = coeffForMs (400.0f), agC = coeffForMs (300.0f);
    const int scSilenceLimit = (int) (5.0 * sr);

    float peakL = 0.0f, peakR = 0.0f, lastLfoDb = 0.0f;
    double sLR = 0.0, sLL = 0.0, sRR = 0.0;

    auto follow = [envAtk, envRel] (float env, float x)
    {
        return x > env ? x + (env - x) * envAtk : x + (env - x) * envRel;
    };

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = stereo ? R[i] : inL;

        // ---- detectors
        float scLevel = 0.0f;
        if (scBus) scLevel = juce::jmax (std::abs (scL[i]), std::abs (scR[i]));
        scSilentSamples = scLevel > 1.0e-4f ? 0 : juce::jmin (scSilentSamples + 1, 1 << 30);
        const bool scUsable = scBus && scSilentSamples < scSilenceLimit;

        scEnv = follow (scEnv, scLevel);
        inEnv = follow (inEnv, juce::jmax (std::abs (inL), std::abs (inR)));

        // ---- pre-delay
        const float d = delaySamples.getNextValue();
        delay.pushSample (0, inL);
        float a = delay.popSample (0, d);
        float b = a;
        if (stereo) { delay.pushSample (1, inR); b = delay.popSample (1, d); }

        preMs = (a * a + b * b) * 0.5f + (preMs - (a * a + b * b) * 0.5f) * msC;

        // ---- filters (always run to keep state warm)
        {
            const float ha = hp1.processSample (0, a), hb = stereo ? hp1.processSample (1, b) : ha;
            const float ha2 = hp2.processSample (0, ha), hb2 = stereo ? hp2.processSample (1, hb) : ha2;
            if (hpOn) { a = steep ? ha2 : ha; b = steep ? hb2 : hb; }

            const float la = lp1.processSample (0, a), lb = stereo ? lp1.processSample (1, b) : la;
            const float la2 = lp2.processSample (0, la), lb2 = stereo ? lp2.processSample (1, lb) : la2;
            if (lpOn) { a = steep ? la2 : la; b = steep ? lb2 : lb; }
        }
        if (! stereo) b = a;

        // ---- mono bass + width
        if (stereo)
        {
            float loA, hiA, loB, hiB;
            crossover.processSample (0, a, loA, hiA);
            crossover.processSample (1, b, loB, hiB);
            if (monoOn)
            {
                const float m = (loA + loB) * 0.5f;
                a = m + hiA; b = m + hiB;
            }

            const float w = widthSm.getNextValue();
            const float mid = (a + b) * 0.5f, side = (a - b) * 0.5f * w;
            a = mid + side; b = mid - side;
        }

        // ---- auto gain: only compensates the level change of filter / width
        postMs = (a * a + b * b) * 0.5f + (postMs - (a * a + b * b) * 0.5f) * msC;
        {
            float target = 1.0f;
            if (autoGainOn && preMs > 1.0e-9f && postMs > 1.0e-12f)
                target = juce::jlimit (0.25f, 4.0f, std::sqrt (preMs / postMs));
            else if (autoGainOn)
                target = autoGainLin;   // hold during silence
            autoGainLin = target + (autoGainLin - target) * agC;
        }
        a *= autoGainLin; b *= autoGainLin;

        // ---- ducker (sidechain)
        {
            float target = 0.0f;
            if (duckOn && scUsable)
            {
                const float key = juce::jlimit (0.0f, 1.0f, (gainToDb (scEnv) - duckThr + 3.0f) / 6.0f);
                target = -duckAmt * key;
            }
            duckDb = target + (duckDb - target) * (target < duckDb ? duckAtkC : duckRelC);
        }

        // ---- tail gate (sidechain, or own input when no sidechain signal)
        {
            float target = 1.0f;
            if (gateOn)
            {
                const float srcDb = gainToDb (scUsable ? scEnv : inEnv);
                if (srcDb > gateThr) gateHold = gateHoldLen;
                else if (gateHold > 0) --gateHold;
                target = gateHold > 0 ? 1.0f : 0.0f;
            }
            gateGain = target + (gateGain - target) * (target > gateGain ? gateOpenC : gateCloseC);
        }

        // ---- tempo LFO (locked to host position)
        float lfoDb = 0.0f;
        if (lfoOn)
        {
            double ph = std::fmod (ppq / lfoBeats, 1.0);
            if (ph < 0.0) ph += 1.0;
            const float phase = (float) ph;
            float depth01 = 0.0f;
            switch (lfoShape)
            {
                case 0:  depth01 = (1.0f - phase) * (1.0f - phase); break;                         // pump
                case 1:  depth01 = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase); break;
                default: depth01 = 1.0f - phase; break;                                              // ramp
            }
            lfoDb = -lfoAmt * depth01;
        }
        lastLfoDb = lfoDb;
        ppq += ppqPerSample;

        const float dyn = dbToGain (duckDb + lfoDb) * gateGain;
        a *= dyn; b *= dyn;

        // ---- mix / output / compare / mono check
        const float mx = mixSm.getNextValue();
        a = inL + (a - inL) * mx;
        b = inR + (b - inR) * mx;

        const float og = outSm.getNextValue();
        a *= og; b *= og;

        const float cmp = compareSm.getNextValue();
        a += (inL - a) * cmp;
        b += (inR - b) * cmp;

        if (monoCheck) { const float m = (a + b) * 0.5f; a = b = m; }

        L[i] = a;
        if (stereo) R[i] = b;

        peakL = juce::jmax (peakL, std::abs (a));
        peakR = juce::jmax (peakR, std::abs (b));
        sLR += (double) a * b; sLL += (double) a * a; sRR += (double) b * b;
    }

    // ---- meters
    if (sLL > 1.0e-9 && sRR > 1.0e-9)
        corrSm = 0.8f * corrSm + 0.2f * (float) (sLR / std::sqrt (sLL * sRR));

    meters.duckDb.store (duckDb);
    meters.lfoDb.store (lastLfoDb);
    meters.gate.store (gateGain);
    meters.autoDb.store (gainToDb (autoGainLin));
    meters.peakL.store (juce::jmax (peakL, meters.peakL.load() * 0.9f));
    meters.peakR.store (juce::jmax (peakR, meters.peakR.load() * 0.9f));
    meters.corr.store (corrSm);
    meters.bpm.store ((float) bpm);
    meters.hostPlaying.store (playing);
    meters.scState.store (! scBus ? 0 : (scSilentSamples < scSilenceLimit ? 2 : 1));
}

juce::AudioProcessorEditor* SendConditionerProcessor::createEditor()
{
    return new SendConditionerEditor (*this);
}

void SendConditionerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SendConditionerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SendConditionerProcessor();
}
