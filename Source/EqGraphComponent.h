#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class JuceEQAudioProcessor;

/**
 * Renders the EQ filter curve and grid labels.
 * Sampling is adaptive: starts with a log-spaced baseline and adds
 * extra points around local extrema so high-Q peaks/notches are drawn accurately.
 */
class EqGraphComponent : public juce::Component, private juce::Timer
{
public:
    explicit EqGraphComponent(JuceEQAudioProcessor&);
    ~EqGraphComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JuceEQAudioProcessor& processor;

    // Buffers for holding EQ curve plot points
    std::vector<double> freqHz; // Current X axis samples (Hz) are sorted in ascending order
    std::vector<double> magLinear; // |H(f)| matches freqHz.size()
    std::vector<double> tempMag; // Temp buffer for holding EQ curve plot points

    // Eq gridspace
    juce::Rectangle<float> eqGridspace;      

    static constexpr double minDb = -30.0;
    static constexpr double maxDb = +30.0;
    static constexpr int leftPad = 50;
    static constexpr int rightPad = 50;
    static constexpr int topPad = 10;
    static constexpr int bottomPad = 24;

    int baseCount = 256; // Baseline log-sample EQ curve plot points number
    int extremaPnts = 32; // For extra plot points around each extrema
    double mergeEps = 1e-6; // Prevents duplicate points or those which are too close

    // Helper functions
    void timerCallback() override;
    void buildBaseFrequencies(); // Build freqHz with baseline log spacing
    void rebuildResponse(); // Recomputes the filter curve with added extrema points

    static juce::String formatHz(double hz);
    static juce::String formatDb(double db);

    float xForFreq(double hz, const juce::Rectangle<float>& into) const;
    float yForDb(double db, const juce::Rectangle<float>& into) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqGraphComponent)
};
