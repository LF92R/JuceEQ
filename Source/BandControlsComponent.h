#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

// Rotary knob with caption and value label 
struct KnobWithLabel : public juce::Component
{
    KnobWithLabel(const juce::String& caption, bool skewForFreq);
    ~KnobWithLabel() override;

    void resized() override;

    std::unique_ptr<juce::Slider> knob;
    std::unique_ptr<juce::Label> captionLabel;
    std::unique_ptr<juce::Label> valueLabel; // clickable to edit

    KnobLNF lnf;
};

// For bypass button (checkmark box) and knob labels for each band's EQ controls
struct BandRow : public juce::Component
{
    BandRow(int bandIndex, JuceEQAudioProcessor& proc);
    void resized() override;

    // Each band gets an enabled checkbox, and 3 knobs (freq, Q, gain)
    juce::ToggleButton enable{ "Band" };
    KnobWithLabel freq{ "Freq", true };
    KnobWithLabel q{ "Q", false };
    KnobWithLabel gain{ "Gain", false };

    // For APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  enableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  freqAttach, qAttach, gainAttach;

private:
    int index = 1;
    JuceEQAudioProcessor& processor;
};

class BandControlsComponent : public juce::Component
{
public:
    explicit BandControlsComponent(JuceEQAudioProcessor& proc);
    static int preferredHeight();
    void resized() override;

private:
    
    static juce::String hzInt(double v) 
    { 
        return juce::String(juce::roundToInt(v)) + " Hz"; 
    }

    JuceEQAudioProcessor& processor;

    // HPF
    juce::ToggleButton hpfEnable{ "HPF" };
    KnobWithLabel hpfFreq{ "Freq", true };
    juce::ComboBox  hpfSlope;
    std::unique_ptr<juce::Label> hpfFreqLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfFreqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfSlopeAttach;

    // LPF
    juce::ToggleButton lpfEnable{ "LPF" };
    KnobWithLabel lpfFreq{ "Freq", true };
    juce::ComboBox lpfSlope;
    std::unique_ptr<juce::Label> lpfFreqLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lpfEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfFreqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lpfSlopeAttach;

    // Bands container
    std::unique_ptr<juce::Component> bandsContainer;
    std::vector<std::unique_ptr<BandRow>> bands;
};