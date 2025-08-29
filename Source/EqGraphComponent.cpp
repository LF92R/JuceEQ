#include "EqGraphComponent.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

using namespace EqConstants; // Using minEqFreq and maxEqFreq from PluginProcessor.h

namespace
{
    inline double linToDb(double g) { return juce::Decibels::gainToDecibels(std::max(1.0e-12, g)); }
}

EqGraphComponent::EqGraphComponent(JuceEQAudioProcessor& proc)
    : processor(proc)
{
    buildBaseFrequencies();
    rebuildResponse();
    startTimerHz(30); 
}

void EqGraphComponent::resized()
{
    auto bounds = getLocalBounds().toFloat();
    eqGridspace = bounds.withTrimmedLeft((float)leftPad)
        .withTrimmedRight((float)rightPad)
        .withTrimmedTop((float)topPad)
        .withTrimmedBottom((float)bottomPad);
}

void EqGraphComponent::timerCallback()
{
    rebuildResponse();
    repaint();
}

void EqGraphComponent::buildBaseFrequencies()
{
    freqHz.clear();
    freqHz.reserve((size_t)baseCount);

    const double f0 = std::log10((double)minEqFreq);
    const double f1 = std::log10((double)maxEqFreq);
    for (int i = 0; i < baseCount; ++i)
    {
        const double t = (double)i / (double)(baseCount - 1);
        const double hz = std::pow(10.0, juce::jmap(t, 0.0, 1.0, f0, f1));
        freqHz.push_back(hz);
    }

    magLinear.assign(freqHz.size(), 1.0);
    tempMag.resize(freqHz.size());
}

void EqGraphComponent::rebuildResponse()
{
    if (freqHz.empty())
        buildBaseFrequencies();

    // Evaluate response on baseline log-spaced grid between minEqFreq and maxEqFreq into tempMag
    tempMag.resize(freqHz.size());
    processor.getFrequencyResponse(freqHz, tempMag);

    // Convert to dB to look for extrema
    std::vector<double> db(freqHz.size());
    for (size_t i = 0; i < tempMag.size(); ++i)
        db[i] = linToDb(tempMag[i]);

    // Find extrema and mark frequency ranges for refinement
    // Done by detecting a sign change of first derivative with >0.05 dB steps
    std::vector<std::pair<double, double>> refineRanges;
    auto sign = [](double x) { return (x > 0) - (x < 0); };
    for (size_t i = 1; i + 1 < db.size(); ++i)
    {
        const double s0 = db[i] - db[i - 1];
        const double s1 = db[i + 1] - db[i];
        if (sign(s0) != sign(s1) && std::abs(db[i] - db[i - 1]) > 0.05 && std::abs(db[i + 1] - db[i]) > 0.05)
        {
            refineRanges.emplace_back(freqHz[i - 1], freqHz[i + 1]);
        }
    }

    // Add more log-spaced EQ curve points near curve extrema
    std::vector<double> refined = freqHz;
    for (auto [fa, fb] : refineRanges)
    {
        const double a = std::log10(fa);
        const double b = std::log10(fb);
        for (int k = 0; k < extremaPnts; ++k)
        {
            double t = (double)k / (double)(extremaPnts - 1);
            refined.push_back(std::pow(10.0, juce::jmap(t, 0.0, 1.0, a, b)));
        }
    }

    // Sort EQ curve points and delete any duplicates
    std::sort(refined.begin(), refined.end());
    const double eps = mergeEps;
    refined.erase(std::unique(refined.begin(), refined.end(), [eps](double a, double b)
        {
            return std::abs(a - b) <= eps * std::max(1.0, std::abs(a));
        }), refined.end());

    // Prevents too many points from occuring
    if (refined.size() > 4096)
        refined.resize(4096);

    // Evaluate final set of EQ curve points
    std::vector<double> finalMag(refined.size());
    processor.getFrequencyResponse(refined, finalMag);

    // Commit finalized EQ curve points
    freqHz.swap(refined);
    magLinear.swap(finalMag);
}

// ---------- Helpers ----------

// Should hz > 1000.0, convert unit from hz to kHz, and adjust numeric value accordingly
juce::String EqGraphComponent::formatHz(double hz)
{
    if (hz >= 1000.0)
        return juce::String(hz / 1000.0, 1) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
}

juce::String EqGraphComponent::formatDb(double db)
{
    if (std::abs(db) < 0.05) return "0 dB";
    return juce::String(db, 0) + " dB";
}

// Maps freq to an X px coord within the EQ grid
// Uses log-spacing 
float EqGraphComponent::xForFreq(double hz, const juce::Rectangle<float>& rectArea) const
{
    const double minLog10Hz = std::log10((double)minEqFreq);
    const double maxLog10Hz = std::log10((double)maxEqFreq);
    const double normXPos = (std::log10(hz) - minLog10Hz) / (maxLog10Hz - minLog10Hz);
    return rectArea.getX() + (float)normXPos * rectArea.getWidth();
}

// Maps dB value to a Y px coord in the EQ grid
float EqGraphComponent::yForDb(double db, const juce::Rectangle<float>& eqGridBounds) const
{
    const double dbValBounds = juce::jlimit(minDb, maxDb, db);
    const double normY = (dbValBounds - minDb) / (maxDb - minDb); // 0 at bottom
    return eqGridBounds.getBottom() - (float)normY * eqGridBounds.getHeight();
}

// ---------- Paint ----------

void EqGraphComponent::paint(juce::Graphics& graphics)
{
    const auto outsideBg = juce::Colours::black; // window background
    const auto plotBg = juce::Colour(0xFF15181A); // inner plot fill
    const auto gridCol = juce::Colour(0xFF262A2E); // grid lines
    const auto frameCol = juce::Colour(0xFF2E3236); // plot border
    const auto textCol = juce::Colour(0xFFB9BEC4); // tick labels
    const auto eqCurveCol = juce::Colours::white;    

    graphics.fillAll(outsideBg);

    // plot background + frame
    graphics.setColour(plotBg);
    graphics.fillRect(eqGridspace);
    graphics.setColour(frameCol);
    graphics.drawRect(eqGridspace, 1.0f);

    // Draw EQ grid
    graphics.setColour(gridCol);
    const double xFreqPos[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (double pos : xFreqPos)
    {
        const float x = xForFreq(pos, eqGridspace);
        graphics.drawVerticalLine((int)std::round(x), eqGridspace.getY(), eqGridspace.getBottom());
    }
    for (int db = (int)minDb; db <= (int)maxDb; db += 6)
    {
        const float y = yForDb((double)db, eqGridspace);
        graphics.drawHorizontalLine((int)std::round(y), eqGridspace.getX(), eqGridspace.getRight());
    }

    // Draw Hz and dB pos values at proper positions
    graphics.setColour(textCol);
    graphics.setFont(12.0f);
    for (double freqPos : xFreqPos)
    {
        const float x = xForFreq(freqPos, eqGridspace);
        graphics.drawFittedText(formatHz(freqPos), juce::Rectangle<int>((int)x - 30, (int)eqGridspace.getBottom(), 60, 16),
            juce::Justification::centred, 1);
    }
    for (int yDbVal = (int)minDb; yDbVal <= (int)maxDb; yDbVal += 6)
    {
        const float y = yForDb((double)yDbVal, eqGridspace);
        graphics.drawFittedText(formatDb((double)yDbVal),
            juce::Rectangle<int>((int)eqGridspace.getRight() + 4, (int)y - 8, 44, 16),
            juce::Justification::centredLeft, 1);
    }

    // Draw Eq response curve 
    if (freqHz.size() == magLinear.size() && !freqHz.empty())
    {
        juce::Path p;
        for (size_t i = 0; i < freqHz.size(); ++i)
        {
            const float x = xForFreq(freqHz[i], eqGridspace);
            const float y = yForDb(juce::Decibels::gainToDecibels(std::max(1.0e-12, magLinear[i])), eqGridspace);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        graphics.setColour(eqCurveCol);
        graphics.strokePath(p, juce::PathStrokeType(2.0f));
    }
}

