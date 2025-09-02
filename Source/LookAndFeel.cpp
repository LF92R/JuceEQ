#include "LookAndFeel.h"
#include <cmath>

void KnobLNF::drawRotarySlider(juce::Graphics& sldrGraphics,
	int x, int y, int width, int height,
	float sliderPosNormalized, float rotaryStartAngle, float rotaryEndAngle, // rotaryStartAngle, rotaryEndAngle in radians
	juce::Slider& slider)
{
	// Knob is a circle with bar indicating the knob position
	juce::ignoreUnused(slider);
	auto knobBounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6.0f);
	auto knobRadius = juce::jmin(knobBounds.getWidth(), knobBounds.getHeight()) * 0.5f;
	auto knobCenter = knobBounds.getCentre();
	auto centerX = knobCenter.x;
	auto centerY = knobCenter.y;
	auto knobAngle = rotaryStartAngle + sliderPosNormalized * (rotaryEndAngle - rotaryStartAngle);

	// Knob background
	sldrGraphics.setColour(juce::Colours::darkgrey);
	sldrGraphics.fillEllipse(centerX - knobRadius, centerY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

	// Knob outer border
	sldrGraphics.setColour(juce::Colours::black.withAlpha(0.9f));
	sldrGraphics.drawEllipse(centerX - knobRadius, centerY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

	// Knob tick indicator
	juce::Path pth;
	const float tickLength = knobRadius * 0.78f;
	pth.addLineSegment({ { centerX, centerY }, { centerX + tickLength * std::cos(knobAngle), centerY + tickLength * std::sin(knobAngle) } }, 2.0f);
	sldrGraphics.setColour(juce::Colours::white);
	sldrGraphics.strokePath(pth, juce::PathStrokeType(2.0f));
}