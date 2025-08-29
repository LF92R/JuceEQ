#include "BandControlsComponent.h"
#include "PluginProcessor.h"
#include <cmath>

using namespace EqConstants;

// Constructor
KnobWithLabel::KnobWithLabel(const juce::String& caption, bool skewForFreq)
{
    knob = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    knob->setLookAndFeel(&lnf);
    knob->setDoubleClickReturnValue(true, 0.0);

    // DO NOT call setSkewFactorFromMidPoint() here - slider uses default range
    // Skew is applied later, AFTER setting the knob range in each place that configures a frequency knob (HPF/LPF/band freq)
    addAndMakeVisible(*knob);

    captionLabel = std::make_unique<juce::Label>();
    captionLabel->setText(caption, juce::dontSendNotification);
    captionLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*captionLabel);

    valueLabel = std::make_unique<juce::Label>();
    valueLabel->setJustificationType(juce::Justification::centred);
    valueLabel->setInterceptsMouseClicks(true, false);
    addAndMakeVisible(*valueLabel);
}

// Destructor
KnobWithLabel::~KnobWithLabel()
{
    if (knob != nullptr)
        knob->setLookAndFeel(nullptr);
}

void KnobWithLabel::resized()
{
    auto localBounds = getLocalBounds();
    auto top = localBounds.removeFromTop(18);

    if (captionLabel)
        captionLabel->setBounds(top);

    auto bottom = localBounds.removeFromBottom(18);

    if (valueLabel)
        valueLabel->setBounds(bottom);

    if (knob)
        knob->setBounds(localBounds.reduced(2));
}

// ----- Helpers -----

// freq set to int at idle
static inline juce::String formatHzIdle(double v)
{
    return juce::String(juce::roundToInt(v)) + " Hz";
}

// Q set to 2 decimal places at idle (same as when editing)
static inline juce::String formatQIdle(double v)
{
    return juce::String(v, 2);
}

// gain set to 1 decimal place at idle
static inline juce::String formatGainIdle(double v)
{
    return juce::String(v, 1) + " dB";
}

// ---------- BandRow ----------
BandRow::BandRow(int bandIndex, JuceEQAudioProcessor& proc) : index(bandIndex), processor(proc)
{
    enable.setButtonText("Band " + juce::String(index));
    addAndMakeVisible(enable);
    addAndMakeVisible(freq);
    addAndMakeVisible(q);
    addAndMakeVisible(gain);

    // Ranges
    freq.knob->setRange(minEqFreq, maxEqFreq, 0.01);
    freq.knob->setSkewFactorFromMidPoint(1000.0); // OK: after range is set

    q.knob->setRange(eqMinQ, eqMaxQ, 0.0001);
    gain.knob->setRange(minEqGainDb, maxEqGainDb, 0.01);

    // Idle value labels and edit behavior for band knobs
    auto makeEditable = [](juce::Label& lbl, juce::Slider& src, bool isIntHz, std::function<juce::String(double)> idleFormat)
        {
            lbl.setEditable(false, true, false);
            lbl.setJustificationType(juce::Justification::centred);

            lbl.onEditorShow = [&lbl, &src]
                {
                    if (auto* editor = lbl.getCurrentTextEditor())
                    {
                        editor->setJustification(juce::Justification::centred);
                        editor->setInputRestrictions(0, "0123456789.-");
                        editor->setText(juce::String(src.getValue(), 2), juce::dontSendNotification); // 2 decimals during editing freq, Q, or gain
                        editor->selectAll();
                    }
                };

            lbl.onTextChange = [&lbl, &src, isIntHz, idleFormat]
                {
                    const auto range = src.getRange();
                    const double val = lbl.getText().getDoubleValue();

                    if (std::isfinite(val))
                        src.setValue(juce::jlimit(range.getStart(), range.getEnd(), val), juce::sendNotificationAsync);
                    
                    // Restore idle formatting
                    if (isIntHz) 
                        lbl.setText(idleFormat(src.getValue()), juce::dontSendNotification);
                    else         
                        lbl.setText(idleFormat(src.getValue()), juce::dontSendNotification);
                };
        };

    freq.knob->onValueChange = [this]()
        {
            if (freq.valueLabel) 
                freq.valueLabel->setText(formatHzIdle(freq.knob->getValue()), juce::dontSendNotification); // freq set to int at idle
        };

    q.knob->onValueChange = [this]()
        {
            if (q.valueLabel) 
                q.valueLabel->setText(formatQIdle(q.knob->getValue()), juce::dontSendNotification); // Q set to 2 decimal places at idle  (same as when editing)
        };

    gain.knob->onValueChange = [this]()
        {
            if (gain.valueLabel) 
                gain.valueLabel->setText(formatGainIdle(gain.knob->getValue()), juce::dontSendNotification); // gain set to 1 decimal place at idle
        };

    // Set initial label text
    freq.knob->onValueChange();
    q.knob->onValueChange();
    gain.knob->onValueChange();

    makeEditable(*freq.valueLabel, *freq.knob, true, formatHzIdle);
    makeEditable(*q.valueLabel, *q.knob, false, formatQIdle);
    makeEditable(*gain.valueLabel, *gain.knob, false, formatGainIdle);

    // APVTS attachments
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, eqBandParamType(index, "enabled"), enable);

    freqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, eqBandParamType(index, "freq"), *freq.knob);

    qAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, eqBandParamType(index, "q"), *q.knob);

    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, eqBandParamType(index, "gain"), *gain.knob);
}

void BandRow::resized()
{
    auto reducedBounds = getLocalBounds().reduced(8);
    auto left = reducedBounds.removeFromLeft(110);
    enable.setBounds(left.removeFromTop(24));
    left.removeFromTop(4);

    const int knobW = 110;
    auto freqBounds = reducedBounds.removeFromLeft(knobW);
    auto qBounds = reducedBounds.removeFromLeft(knobW);
    auto gainBounds = reducedBounds.removeFromLeft(knobW);

    freq.setBounds(freqBounds);
    q.setBounds(qBounds);
    gain.setBounds(gainBounds);
}

// ---------- BandControlsComponent ----------
BandControlsComponent::BandControlsComponent(JuceEQAudioProcessor& proc) : processor(proc)
{
    // HPF
    addAndMakeVisible(hpfEnable);
    addAndMakeVisible(hpfFreq);
    addAndMakeVisible(hpfSlope);

    hpfEnable.setButtonText("HPF");
    hpfSlope.addItem("6", 1);
    hpfSlope.addItem("12", 2);
    hpfSlope.addItem("24", 3);
    hpfSlope.addItem("48", 4);

    hpfFreq.knob->setRange(minEqFreq, maxEqFreq, 0.01);
    hpfFreq.knob->setSkewFactorFromMidPoint(1000.0); 

    hpfFreq.knob->onValueChange = [this]()
        {
            if (hpfFreq.valueLabel) hpfFreq.valueLabel->setText(hzInt(hpfFreq.knob->getValue()), juce::dontSendNotification);
        };
    hpfFreq.knob->onValueChange();

    if (hpfFreq.valueLabel)
    {
        hpfFreq.valueLabel->setEditable(false, true, false);
        hpfFreq.valueLabel->onEditorShow = [this]()
            {
                if (auto* txtEditor = hpfFreq.valueLabel->getCurrentTextEditor())
                {
                    txtEditor->setJustification(juce::Justification::centred);
                    txtEditor->setInputRestrictions(0, "0123456789.-");
                    txtEditor->setText(juce::String(hpfFreq.knob->getValue(), 2), juce::dontSendNotification);
                    txtEditor->selectAll();
                }
            };

        hpfFreq.valueLabel->onTextChange = [this]()
            {
                const auto range = hpfFreq.knob->getRange();
                const double val = hpfFreq.valueLabel->getText().getDoubleValue();
                if (std::isfinite(val))
                    hpfFreq.knob->setValue(juce::jlimit(range.getStart(), range.getEnd(), val), juce::sendNotificationAsync);
                if (hpfFreq.valueLabel) hpfFreq.valueLabel->setText(hzInt(hpfFreq.knob->getValue()), juce::dontSendNotification);
            };
    }

    // LPF
    addAndMakeVisible(lpfEnable);
    addAndMakeVisible(lpfFreq);
    addAndMakeVisible(lpfSlope);

    lpfEnable.setButtonText("LPF");
    lpfSlope.addItem("6", 1);
    lpfSlope.addItem("12", 2);
    lpfSlope.addItem("24", 3);
    lpfSlope.addItem("48", 4);

    lpfFreq.knob->setRange(minEqFreq, maxEqFreq, 0.01);
    lpfFreq.knob->setSkewFactorFromMidPoint(1000.0); // OK: after range is set

    lpfFreq.knob->onValueChange = [this]()
        {
            if (lpfFreq.valueLabel) lpfFreq.valueLabel->setText(hzInt(lpfFreq.knob->getValue()), juce::dontSendNotification);
        };
    lpfFreq.knob->onValueChange();

    if (lpfFreq.valueLabel)
    {
        lpfFreq.valueLabel->setEditable(false, true, false);
        lpfFreq.valueLabel->onEditorShow = [this]()
            {
                if (auto* txtEditor = lpfFreq.valueLabel->getCurrentTextEditor())
                {
                    txtEditor->setJustification(juce::Justification::centred);
                    txtEditor->setInputRestrictions(0, "0123456789.-");
                    txtEditor->setText(juce::String(lpfFreq.knob->getValue(), 2), juce::dontSendNotification);
                    txtEditor->selectAll();
                }
            };

        lpfFreq.valueLabel->onTextChange = [this]()
            {
                const auto range = lpfFreq.knob->getRange();
                const double val = lpfFreq.valueLabel->getText().getDoubleValue();
                if (std::isfinite(val))
                    lpfFreq.knob->setValue(juce::jlimit(range.getStart(), range.getEnd(), val), juce::sendNotificationAsync);
                if (lpfFreq.valueLabel) lpfFreq.valueLabel->setText(hzInt(lpfFreq.knob->getValue()), juce::dontSendNotification);
            };
    }

    // Attach HPF/LPF
    hpfEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "hpfEnabled", hpfEnable);
    hpfFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "hpfFreq", *hpfFreq.knob);
    hpfSlopeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "hpfSlope", hpfSlope);

    lpfEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "lpfEnabled", lpfEnable);
    lpfFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "lpfFreq", *lpfFreq.knob);
    lpfSlopeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "lpfSlope", lpfSlope);

    // Bands container and rows
    bandsContainer = std::make_unique<juce::Component>();
    addAndMakeVisible(*bandsContainer);

    bands.reserve(maxEqBands);
    for (int i = 1; i <= maxEqBands; ++i)
    {
        auto row = std::make_unique<BandRow>(i, processor);
        bandsContainer->addAndMakeVisible(*row);
        bands.push_back(std::move(row));
    }
}

int BandControlsComponent::preferredHeight()
{
    const int lpfHpfRow = 100; // HPF and LPF on a single row
    const int rowH = 110;
    const int bandRows = (maxEqBands + 1) / 2; // Two band EQ controls per row
    return lpfHpfRow + bandRows * rowH + 16;
}

void BandControlsComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    // HPF and LPF on a single row
    {
        auto topRow = bounds.removeFromTop(100);

        auto leftHalf = topRow.removeFromLeft(topRow.getWidth() / 2);
        auto rightHalf = topRow;

        // HPF bounds
        {
            auto left = leftHalf.removeFromLeft(110);
            hpfEnable.setBounds(left.removeFromTop(24));

            const int knobWidth = 110;
            auto hpfFreqBounds = leftHalf.removeFromLeft(knobWidth);
            hpfFreq.setBounds(hpfFreqBounds);

            leftHalf.removeFromLeft(12);
            auto slopeArea = leftHalf.removeFromLeft(80);
            hpfSlope.setBounds(slopeArea.withTrimmedTop(24).withHeight(24));
        }

        // LPF bounds
        {
            auto left = rightHalf.removeFromLeft(110);
            lpfEnable.setBounds(left.removeFromTop(24));

            const int knobW = 110;
            auto lpfFreqBounds = rightHalf.removeFromLeft(knobW);
            lpfFreq.setBounds(lpfFreqBounds);

            rightHalf.removeFromLeft(12);
            auto slopeArea = rightHalf.removeFromLeft(80);
            lpfSlope.setBounds(slopeArea.withTrimmedTop(24).withHeight(24));
        }
    }

    bounds.removeFromTop(8);

    // Two bands per row
    if (bandsContainer)
    {
        bandsContainer->setBounds(bounds);

        const int rowH = 110;
        const int colW = bandsContainer->getWidth() / 2;
        auto area = bandsContainer->getLocalBounds();

        int x = 0, y = 0;
        for (size_t i = 0; i < bands.size(); ++i)
        {
            juce::Rectangle<int> bandCtrlArea = { x * colW, y * rowH, colW, rowH };
            bands[i]->setBounds(bandCtrlArea.reduced(4));

            // Move to next row after 2 band Eq controls have been put into a row
            if (++x >= 2) 
            { 
                x = 0; ++y; 
            }
        }
    }
}

