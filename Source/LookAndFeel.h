#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Provides an AppLookAndFeel alias so the plugin editor can declare "AppLookAndFeel lnf;"
using AppLookAndFeel = juce::LookAndFeel_V4;

// Basic rotary knob used by BandControlsComponent
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
