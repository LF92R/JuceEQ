#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "EqGraphComponent.h"
#include "BandControlsComponent.h"

// Layout constants for I/O rails
// Ensure faderWidth >= textBoxWidth to prevent clipping the I/O sliders' text boxes
namespace
{
    constexpr int textBoxWidth = 64; // width of slider value box under the fader
    constexpr int textBoxHeight = 18; // height of that value box
    constexpr int faderWidth = 64; // width granted to the slider (>= textBoxWidth)
    constexpr int railPadding = 8; // side padding inside each rail
    constexpr int labelHeight = 18; // static caption (“Input” / “Output”) under the rail
    constexpr int railWidth = faderWidth + railPadding * 2; // total rail slice width
}

// Set fader style - fader with text box below
static void styleVerticalFader(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight); // TextBox is drawn INSIDE the slider's bounds!
    s.setRange(-60.0, 10.0, 0.01); // This is only the text box's visual range. Their actual value comes from the APVTS attachment.
    s.setNumDecimalPlacesToDisplay(1);
    s.setTextValueSuffix(" dB");
}

JuceEQAudioProcessorEditor::JuceEQAudioProcessorEditor(JuceEQAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    styleVerticalFader(inGain);
    styleVerticalFader(outGain);
    addAndMakeVisible(inGain);
    addAndMakeVisible(outGain);

    // Label for I/O sliders
    inputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setJustificationType(juce::Justification::centred);
    inputLabel.setInterceptsMouseClicks(false, false);
    outputLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(inputLabel);
    addAndMakeVisible(outputLabel);

    // apvts attachments to input and output gain sliders
    inAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "inGain", inGain);
    outAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "outGain", outGain);

    // Instantiates EQ graph and gridlines
    graph = std::make_unique<EqGraphComponent>(processor);
    addAndMakeVisible(*graph);

    // For HPF, LPF, and EQ band controls - set in a scrollable viewport
    bandControls = std::make_unique<BandControlsComponent>(processor);
    controlsViewport.setViewedComponent(bandControls.get(), false);
    controlsViewport.setScrollBarsShown(true, false);
    controlsViewport.setScrollOnDragEnabled(true);
    addAndMakeVisible(controlsViewport);

    setResizable(true, true);
    setSize(1250, 760);
}

JuceEQAudioProcessorEditor::~JuceEQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

// For background only
void JuceEQAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::black);
}

void JuceEQAudioProcessorEditor::resized()
{
    // For 8px outer margins
    auto bounds = getLocalBounds().reduced(8);
    if (bounds.isEmpty()) 
        return;

    // Left/Right rails for I/O faders (+ static labels underneath).
    auto leftRail = bounds.removeFromLeft(railWidth);
    auto rightRail = bounds.removeFromRight(railWidth);

    // Leave space for the static “Input/Output” label under each rail
    auto leftLabelArea = leftRail.removeFromBottom(labelHeight);
    auto rightLabelArea = rightRail.removeFromBottom(labelHeight);
    inputLabel.setBounds(leftLabelArea);
    outputLabel.setBounds(rightLabelArea);

    // Pad the rails so the fader isn't flush against the edges
    leftRail = leftRail.reduced(railPadding, 0);
    rightRail = rightRail.reduced(railPadding, 0);

    // Give each fader a rectangle that's wide enough for its TextBox (no clipping).
    // The Slider draws its own value box at the bottom INSIDE these bounds.
    auto leftFaderArea = leftRail.withWidth(faderWidth);
    auto rightFaderArea = rightRail.withWidth(faderWidth);
    inGain.setBounds(leftFaderArea);
    outGain.setBounds(rightFaderArea);

    // For EQ graph and EQ filter & band controls
    auto bottom = bounds.removeFromBottom(280);
    graph->setBounds(bounds);

    controlsViewport.setBounds(bottom);
    const int prefH = BandControlsComponent::preferredHeight();
    const int visibleW = juce::jmax(100, controlsViewport.getMaximumVisibleWidth());
    bandControls->setSize(visibleW, prefH);
}

juce::AudioProcessorEditor* createEQEditor(JuceEQAudioProcessor& p)
{
    return new JuceEQAudioProcessorEditor(p);
}
