#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class SCLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SCLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class SendConditionerEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit SendConditionerEditor (SendConditionerProcessor&);
    ~SendConditionerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> att;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> att;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<APVTS::ComboBoxAttachment> att;
    };
    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    SendConditionerProcessor& proc;
    SCLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    std::vector<std::unique_ptr<Knob>>   knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Combo>>  combos;
    Panel panels[6];

    // controls by section (non-owning)
    Toggle *pdSync {}, *duckOn {}, *gateOn {}, *lfoOn {}, *autoGain {}, *monoCheck {}, *compare {};
    Combo  *pdNote {}, *slope {}, *gateLen {}, *lfoRate {}, *lfoShape {};
    Knob   *pdMs {}, *hpf {}, *lpf {}, *monoLow {}, *width {};
    Knob   *duckThr {}, *duckAmt {}, *duckAtk {}, *duckRel {};
    Knob   *gateThr {}, *gateRel {}, *lfoAmt {}, *mix {}, *outGain {};

    // meter areas
    juce::Rectangle<int> headerArea, duckMeter, gateMeter, lfoMeter, levelMeter, corrMeter, autoArea;

    // meter values (message thread copies)
    float mDuck = 0, mLfo = 0, mGate = 1, mAuto = 0, mPeakL = 0, mPeakR = 0, mCorr = 1, mBpm = 120;
    int mSc = 0;
    bool mPlaying = false;

    Knob*   addKnob   (const char* id, const juce::String& text, const juce::String& tip = {});
    Toggle* addToggle (const char* id, const juce::String& text, const juce::String& tip = {});
    Combo*  addCombo  (const char* id, const juce::String& text, const juce::String& tip = {});

    void updateEnablement();
    void timerCallback() override;

    void drawHBar (juce::Graphics&, juce::Rectangle<int>, const juce::String& caption,
                   float value01, juce::Colour, const juce::String& valueText) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendConditionerEditor)
};
