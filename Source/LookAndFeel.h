#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Provide an AppLookAndFeel alias so editor code can declare `AppLookAndFeel lnf;`
using AppLookAndFeel = juce::LookAndFeel_V4;

// Basic rotary knob look-and-feel 
// Used by BandControls
struct KnobLNF : public juce::LookAndFeel_V4
{
    KnobLNF() = default;
    ~KnobLNF() override = default;

    void drawRotarySlider(juce::Graphics& sldrGraphics,
        int x, int y, int width, int height,
        float sliderPosNormalized,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider) override;
};
