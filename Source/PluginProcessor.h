#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"

/** Put this AFTER the reverb/delay on an FX (send) channel.
    Chain: pre-delay -> HPF/LPF -> mono-bass -> width -> auto gain
           -> ducker x tempo LFO x tail gate -> mix -> output -> compare -> mono check */
class SendConditionerProcessor : public juce::AudioProcessor
{
public:
    SendConditionerProcessor();

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    /** Values for the editor's meters (written by the audio thread). */
    struct Meters
    {
        std::atomic<float> duckDb { 0.0f }, lfoDb { 0.0f }, gate { 1.0f }, autoDb { 0.0f };
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, corr { 1.0f }, bpm { 120.0f };
        std::atomic<int>   scState { 0 };   // 0 = sidechain off, 1 = on but silent, 2 = receiving
        std::atomic<bool>  hostPlaying { false };
    };
    Meters meters;

    static constexpr double maxPredelaySeconds = 3.0;

private:
    struct P
    {
        std::atomic<float>* pdSync; std::atomic<float>* pdNote; std::atomic<float>* pdMs;
        std::atomic<float>* hpf; std::atomic<float>* lpf; std::atomic<float>* slope;
        std::atomic<float>* monoLow; std::atomic<float>* width;
        std::atomic<float>* duckOn; std::atomic<float>* duckThr; std::atomic<float>* duckAmt;
        std::atomic<float>* duckAtk; std::atomic<float>* duckRel;
        std::atomic<float>* gateOn; std::atomic<float>* gateThr; std::atomic<float>* gateLen;
        std::atomic<float>* gateRel;
        std::atomic<float>* lfoOn; std::atomic<float>* lfoRate; std::atomic<float>* lfoShape;
        std::atomic<float>* lfoAmt;
        std::atomic<float>* autoGain; std::atomic<float>* mix; std::atomic<float>* outGain;
        std::atomic<float>* monoCheck; std::atomic<float>* compare;
    } p {};

    double sr = 44100.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::SmoothedValue<float> delaySamples, mixSm, outSm, widthSm, compareSm;

    juce::dsp::StateVariableTPTFilter<float> hp1, hp2, lp1, lp2;
    juce::dsp::LinkwitzRileyFilter<float> crossover;

    float scEnv = 0.0f, inEnv = 0.0f;
    float duckDb = 0.0f, gateGain = 1.0f, autoGainLin = 1.0f;
    float preMs = 0.0f, postMs = 0.0f;
    int   gateHold = 0;
    int   scSilentSamples = 1 << 30;
    double ppq = 0.0;
    float corrSm = 1.0f;

    template <typename T> T param (std::atomic<float>* a) const { return (T) a->load(); }
    static bool isOn (std::atomic<float>* a) { return a->load() > 0.5f; }
    float coeffForMs (float ms) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendConditionerProcessor)
};
