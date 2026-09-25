#include "PluginEditor.h"

namespace
{
    juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    const juce::Colour bg       { 0xff1b1e23 };
    const juce::Colour panelBg  { 0xff252a31 };
    const juce::Colour panelEdge{ 0xff343a43 };
    const juce::Colour textCol  { 0xffe4e6ea };
    const juce::Colour dimText  { 0xff9aa3ad };
    const juce::Colour accent   { 0xff5fb3ff };
    const juce::Colour duckCol  { 0xffffa94d };
    const juce::Colour gateCol  { 0xff7bd88f };
    const juce::Colour lfoCol   { 0xffc792ea };

    juce::String pickFontName()
    {
        static const juce::String name = []
        {
            const auto names = juce::Font::findAllTypefaceNames();
            for (auto* n : { "Yu Gothic UI", "Meiryo UI", "Meiryo", "MS UI Gothic",
                             "Hiragino Sans", "Noto Sans CJK JP", "Noto Sans JP" })
                if (names.contains (n)) return juce::String (n);
            return juce::String();
        }();
        return name;
    }

    constexpr int editorW = 960, editorH = 560, headerH = 36, gap = 8;
    constexpr int knobW = 72, knobH = 88, rowH = 24;
}

//==============================================================================
SCLookAndFeel::SCLookAndFeel()
{
    if (const auto f = pickFontName(); f.isNotEmpty())
        setDefaultSansSerifTypefaceName (f);

    setColour (juce::Slider::textBoxTextColourId, textCol);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, dimText);
    setColour (juce::ToggleButton::textColourId, textCol);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1e2227));
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::textColourId, textCol);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff22262c));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
}

void SCLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lineW = juce::jmax (2.5f, radius * 0.16f);
    const float arcR = radius - lineW * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (panelEdge);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto col = s.isEnabled() ? accent : dimText.withAlpha (0.5f);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const juce::Point<float> tip (centre.x + (arcR - lineW) * std::sin (angle),
                                  centre.y - (arcR - lineW) * std::cos (angle));
    g.setColour (s.isEnabled() ? textCol : dimText);
    g.drawLine ({ centre, tip }, 2.0f);
}

//==============================================================================
SendConditionerEditor::Knob* SendConditionerEditor::addKnob (const char* id, const juce::String& text,
                                                               const juce::String& tip)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
    k->slider.setTooltip (tip);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

SendConditionerEditor::Toggle* SendConditionerEditor::addToggle (const char* id, const juce::String& text,
                                                                   const juce::String& tip)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    t->button.setTooltip (tip);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, t->button);
    toggles.push_back (std::move (t));
    return toggles.back().get();
}

SendConditionerEditor::Combo* SendConditionerEditor::addCombo (const char* id, const juce::String& text,
                                                                 const juce::String& tip)
{
    auto c = std::make_unique<Combo>();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (choice->choices, 1);
    c->box.setTooltip (tip);
    c->label.setText (text, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c->box);
    combos.push_back (std::move (c));
    return combos.back().get();
}

SendConditionerEditor::SendConditionerEditor (SendConditionerProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    panels[0].title = jp (u8"プリディレイ");
    panels[1].title = jp (u8"フィルター / ステレオ幅");
    panels[2].title = jp (u8"ダッカー（サイドチェーン）");
    panels[3].title = jp (u8"テール・ゲート");
    panels[4].title = jp (u8"テンポLFO");
    panels[5].title = jp (u8"出力");

    // Pre-delay
    pdSync = addToggle (scp::pdSync, jp (u8"テンポ同期"), jp (u8"ONで音価、OFFでミリ秒指定"));
    pdNote = addCombo  (scp::pdNote, jp (u8"音価"));
    pdMs   = addKnob   (scp::pdMs,   jp (u8"時間"), jp (u8"テンポ同期OFF時のプリディレイ"));

    // Filter / width
    hpf     = addKnob  (scp::hpf,     "HPF", jp (u8"低域カット（左いっぱいでOFF）"));
    lpf     = addKnob  (scp::lpf,     "LPF", jp (u8"高域カット（右いっぱいでOFF）"));
    monoLow = addKnob  (scp::monoLow, jp (u8"低域モノ"), jp (u8"この周波数以下をモノラル化（左いっぱいでOFF）"));
    width   = addKnob  (scp::width,   jp (u8"幅"), jp (u8"0%=モノ / 100%=そのまま / 200%=ワイド"));
    slope   = addCombo (scp::slope,   jp (u8"傾き"));

    // Ducker
    duckOn  = addToggle (scp::duckOn,  "ON", jp (u8"サイドチェーン入力が鳴っている間、返りを下げる"));
    duckThr = addKnob   (scp::duckThr, jp (u8"しきい値"));
    duckAmt = addKnob   (scp::duckAmt, jp (u8"深さ"));
    duckAtk = addKnob   (scp::duckAtk, jp (u8"アタック"));
    duckRel = addKnob   (scp::duckRel, jp (u8"リリース"));

    // Gate
    gateOn  = addToggle (scp::gateOn,  "ON", jp (u8"信号がしきい値を下回ってから、ホールド長の後に残響を切る。"
                                                 u8"サイドチェーン受信中はその信号で判定"));
    gateLen = addCombo  (scp::gateLen, jp (u8"ホールド"));
    gateThr = addKnob   (scp::gateThr, jp (u8"しきい値"));
    gateRel = addKnob   (scp::gateRel, jp (u8"切れ方"), jp (u8"閉じる速さ"));

    // LFO
    lfoOn    = addToggle (scp::lfoOn,    "ON", jp (u8"曲のテンポに合わせて返りを周期的に下げる"));
    lfoRate  = addCombo  (scp::lfoRate,  jp (u8"周期"));
    lfoShape = addCombo  (scp::lfoShape, jp (u8"形"));
    lfoAmt   = addKnob   (scp::lfoAmt,   jp (u8"深さ"));

    // Output
    autoGain  = addToggle (scp::autoGain,  jp (u8"自動ゲイン"), jp (u8"フィルター/幅で変わった音量だけを補正"
                                                                 u8"（ダッカー・ゲート・LFOの効果は補正しない）"));
    monoCheck = addToggle (scp::monoCheck, jp (u8"モノ確認"), jp (u8"出力をモノラルにして確認"));
    compare   = addToggle (scp::compare,   jp (u8"素通し比較"), jp (u8"処理前の音と聴き比べる"));
    mix       = addKnob   (scp::mix,       "Mix", jp (u8"処理前の音との混ぜ具合"));
    outGain   = addKnob   (scp::outGain,   jp (u8"出力"));

    // hook up enablement only after every control exists
    // (attachments fire a click while the toggles are being created)
    for (auto& t : toggles)
        t->button.onClick = [this] { updateEnablement(); };

    setSize (editorW, editorH);
    updateEnablement();
    startTimerHz (30);
}

SendConditionerEditor::~SendConditionerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SendConditionerEditor::updateEnablement()
{
    if (pdSync == nullptr || pdNote == nullptr || pdMs == nullptr || duckOn == nullptr
         || duckThr == nullptr || duckAmt == nullptr || duckAtk == nullptr || duckRel == nullptr
         || gateOn == nullptr || gateThr == nullptr || gateRel == nullptr || gateLen == nullptr
         || lfoOn == nullptr || lfoAmt == nullptr || lfoRate == nullptr || lfoShape == nullptr)
        return;

    const bool sync = pdSync->button.getToggleState();
    pdNote->box.setEnabled (sync);
    pdMs->slider.setEnabled (! sync);

    for (auto* k : { duckThr, duckAmt, duckAtk, duckRel }) k->slider.setEnabled (duckOn->button.getToggleState());
    gateThr->slider.setEnabled (gateOn->button.getToggleState());
    gateRel->slider.setEnabled (gateOn->button.getToggleState());
    gateLen->box.setEnabled (gateOn->button.getToggleState());
    lfoAmt->slider.setEnabled (lfoOn->button.getToggleState());
    lfoRate->box.setEnabled (lfoOn->button.getToggleState());
    lfoShape->box.setEnabled (lfoOn->button.getToggleState());
}

void SendConditionerEditor::timerCallback()
{
    auto& m = proc.meters;
    mDuck = m.duckDb.load(); mLfo = m.lfoDb.load(); mGate = m.gate.load(); mAuto = m.autoDb.load();
    mPeakL = m.peakL.load(); mPeakR = m.peakR.load(); mCorr = m.corr.load(); mBpm = m.bpm.load();
    mSc = m.scState.load(); mPlaying = m.hostPlaying.load();

    // parameters may change from automation / preset recall
    updateEnablement();

    for (auto r : { headerArea, duckMeter, gateMeter, lfoMeter, levelMeter, corrMeter, autoArea })
        repaint (r);
}

//==============================================================================
void SendConditionerEditor::drawHBar (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& caption,
                                      float value01, juce::Colour col, const juce::String& valueText) const
{
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (dimText);
    auto cap = r.removeFromLeft (70);
    g.drawText (caption, cap, juce::Justification::centredLeft);
    auto txt = r.removeFromRight (70);
    g.setColour (textCol);
    g.drawText (valueText, txt, juce::Justification::centredRight);

    auto bar = r.reduced (4, 6).toFloat();
    g.setColour (juce::Colour (0xff15181c));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (col);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, value01)), 3.0f);
}

void SendConditionerEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // header
    g.setColour (juce::Colour (0xff15181c));
    g.fillRect (headerArea);
    g.setColour (textCol);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("Send Conditioner", headerArea.withTrimmedLeft (12), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.0f));
    const juce::String scText = mSc == 0 ? jp (u8"サイドチェーン: 未使用")
                              : mSc == 1 ? jp (u8"サイドチェーン: 信号なし")
                                         : jp (u8"サイドチェーン: 受信中");
    g.setColour (mSc == 2 ? gateCol : dimText);
    g.drawText (scText, headerArea.withTrimmedRight (170), juce::Justification::centredRight);
    g.setColour (dimText);
    g.drawText (juce::String (mBpm, 1) + " BPM" + (mPlaying ? jp (u8"  ▶") : juce::String ("  ■")),
                headerArea.withTrimmedRight (14), juce::Justification::centredRight);

    // panels
    for (auto& pnl : panels)
    {
        g.setColour (panelBg);
        g.fillRoundedRectangle (pnl.bounds.toFloat(), 6.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (pnl.bounds.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (textCol);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (pnl.title, pnl.bounds.reduced (10, 6).removeFromTop (20), juce::Justification::centredLeft);
    }

    // meters
    drawHBar (g, duckMeter, jp (u8"減衰"), -mDuck / 40.0f, duckCol, juce::String (mDuck, 1) + " dB");
    drawHBar (g, gateMeter, jp (u8"ゲート"), mGate, gateCol, mGate > 0.5f ? jp (u8"開") : jp (u8"閉"));
    drawHBar (g, lfoMeter,  jp (u8"減衰"), -mLfo / 24.0f, lfoCol, juce::String (mLfo, 1) + " dB");

    {
        auto r = levelMeter;
        const auto toX = [] (float peak) { return juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (juce::jmax (peak, 1.0e-5f)) + 60.0f) / 60.0f); };
        drawHBar (g, r.removeFromTop (r.getHeight() / 2), "L", toX (mPeakL), accent,
                  juce::String (20.0f * std::log10 (juce::jmax (mPeakL, 1.0e-5f)), 1));
        drawHBar (g, r, "R", toX (mPeakR), accent,
                  juce::String (20.0f * std::log10 (juce::jmax (mPeakR, 1.0e-5f)), 1));
    }

    {
        auto r = corrMeter;
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (dimText);
        g.drawText (jp (u8"位相相関"), r.removeFromLeft (70), juce::Justification::centredLeft);
        auto txt = r.removeFromRight (70);
        g.setColour (mCorr < 0.0f ? juce::Colour (0xffff6b6b) : textCol);
        g.drawText (juce::String (mCorr, 2), txt, juce::Justification::centredRight);

        auto bar = r.reduced (4, 6).toFloat();
        g.setColour (juce::Colour (0xff15181c));
        g.fillRoundedRectangle (bar, 3.0f);
        const float cx = bar.getCentreX();
        const float x = bar.getX() + (juce::jlimit (-1.0f, 1.0f, mCorr) * 0.5f + 0.5f) * bar.getWidth();
        g.setColour (mCorr < 0.0f ? juce::Colour (0xffff6b6b) : gateCol);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, x), bar.getY(), juce::jmax (cx, x), bar.getBottom()));
        g.setColour (dimText);
        g.drawVerticalLine ((int) cx, bar.getY(), bar.getBottom());
    }

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (dimText);
    g.drawText (jp (u8"自動ゲイン補正: ") + juce::String (mAuto, 1) + " dB", autoArea, juce::Justification::centredLeft);
}

void SendConditionerEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (headerH);
    area.reduce (gap, gap);

    const int colW = (area.getWidth() - gap * 2) / 3;
    const int rowHt = (area.getHeight() - gap) / 2;
    for (int i = 0; i < 6; ++i)
        panels[i].bounds = { area.getX() + (i % 3) * (colW + gap), area.getY() + (i / 3) * (rowHt + gap), colW, rowHt };

    auto inner = [] (const Panel& pnl) { return pnl.bounds.reduced (10, 8).withTrimmedTop (24); };

    auto placeKnobs = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> ks)
    {
        const int w = row.getWidth() / (int) ks.size();
        for (auto* k : ks)
        {
            auto cell = row.removeFromLeft (w).withSizeKeepingCentre (knobW, knobH);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell);
        }
    };

    auto placeCombo = [] (juce::Rectangle<int> row, Combo* c, int labelW)
    {
        c->label.setBounds (row.removeFromLeft (labelW));
        c->box.setBounds (row.reduced (0, 1));
    };

    // Pre-delay
    {
        auto r = inner (panels[0]);
        pdSync->button.setBounds (r.removeFromTop (rowH));
        r.removeFromTop (4);
        placeCombo (r.removeFromTop (rowH).withWidth (190), pdNote, 50);
        r.removeFromTop (6);
        placeKnobs (r.removeFromTop (knobH), { pdMs });
    }
    // Filter / width
    {
        auto r = inner (panels[1]);
        placeCombo (r.removeFromTop (rowH).withWidth (190), slope, 50);
        r.removeFromTop (10);
        placeKnobs (r.removeFromTop (knobH), { hpf, lpf, monoLow, width });
    }
    // Ducker
    {
        auto r = inner (panels[2]);
        duckOn->button.setBounds (r.removeFromTop (rowH).withWidth (80));
        r.removeFromTop (4);
        placeKnobs (r.removeFromTop (knobH), { duckThr, duckAmt, duckAtk, duckRel });
        r.removeFromTop (6);
        duckMeter = r.removeFromTop (26);
    }
    // Gate
    {
        auto r = inner (panels[3]);
        auto top = r.removeFromTop (rowH);
        gateOn->button.setBounds (top.removeFromLeft (80));
        placeCombo (top.withWidth (170), gateLen, 64);
        r.removeFromTop (4);
        placeKnobs (r.removeFromTop (knobH), { gateThr, gateRel });
        r.removeFromTop (6);
        gateMeter = r.removeFromTop (26);
    }
    // LFO
    {
        auto r = inner (panels[4]);
        auto top = r.removeFromTop (rowH);
        lfoOn->button.setBounds (top.removeFromLeft (80));
        placeCombo (top.withWidth (160), lfoRate, 44);
        r.removeFromTop (4);
        auto mid = r.removeFromTop (knobH);
        auto shapeRow = mid.removeFromLeft (mid.getWidth() / 2).withSizeKeepingCentre (130, rowH);
        placeCombo (shapeRow, lfoShape, 30);
        placeKnobs (mid, { lfoAmt });
        r.removeFromTop (6);
        lfoMeter = r.removeFromTop (26);
    }
    // Output
    {
        auto r = inner (panels[5]);
        auto top = r.removeFromTop (rowH);
        const int tw = top.getWidth() / 3;
        autoGain->button.setBounds (top.removeFromLeft (tw));
        monoCheck->button.setBounds (top.removeFromLeft (tw));
        compare->button.setBounds (top);
        r.removeFromTop (2);
        placeKnobs (r.removeFromTop (knobH), { mix, outGain });
        r.removeFromTop (2);
        levelMeter = r.removeFromTop (44);
        corrMeter  = r.removeFromTop (24);
        autoArea   = r.removeFromTop (20);
    }
}
