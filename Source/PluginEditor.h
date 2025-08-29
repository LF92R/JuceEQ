#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

class JuceEQAudioProcessor;
class EqGraphComponent;
class BandControlsComponent;

class JuceEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit JuceEQAudioProcessorEditor(JuceEQAudioProcessor&);
    ~JuceEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JuceEQAudioProcessor& processor;
    AppLookAndFeel lnf;

    juce::Slider inGain{ juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::Slider outGain{ juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAttach, outAttach;

    juce::Label inputLabel{ {}, "Input" };
    juce::Label outputLabel{ {}, "Output" };

    std::unique_ptr<EqGraphComponent> graph;
    juce::Viewport controlsViewport;
    std::unique_ptr<BandControlsComponent> bandControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JuceEQAudioProcessorEditor)
};

juce::AudioProcessorEditor* createEQEditor(JuceEQAudioProcessor&);
