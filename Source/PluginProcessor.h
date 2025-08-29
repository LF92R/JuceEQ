#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

namespace EqConstants
{
    constexpr int maxEqBands = 8; // Default is 3, max is 8

    // min and max freq for all freq knobs
    constexpr float minEqFreq = 10.0f;
    constexpr float maxEqFreq = 20000.0f;

    // min and max gain for EQ bands
    constexpr float minEqGainDb = -30.0f;
    constexpr float maxEqGainDb = +30.0f;

    // min and max Q for EQ bands
    constexpr float eqMinQ = 0.10f;
    constexpr float eqMaxQ = 40.0f;
}

// For the 8 (max) EQ bands' knob IDs 
// i.e. eqBandParamType(3, "gain") -> "b3_gain".
static inline juce::String eqBandParamType(int bandIndex, const juce::String& paramType)
{
    return "b" + juce::String(bandIndex) + "_" + paramType;
}

class JuceEQAudioProcessor : public juce::AudioProcessor
{
public:
    using IIRBiquadCoeffs = juce::dsp::IIR::Coefficients<float>; // Biquad coeff holder
    using IIRBiquadCoeffPtr = IIRBiquadCoeffs::Ptr; // Reference-counted pointer to coeffs

    JuceEQAudioProcessor();
    ~JuceEQAudioProcessor() override = default;

    // ----- JUCE boilerplate -----

    const juce::String getName() const override { return "JuceEQ"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // JUCE virtuals
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // For factory presets
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // For JUCE's parameter system, AudioProcessorValueTreeState - stores Ids, ranges, default values, etc. 
    // Syncs UI to DSP via "attachments", and stores state info
    juce::AudioProcessorValueTreeState apvts;

    /* Computes freq response from parameter values to avoid races
     * freqs - vector of frequencies that're being evaluated
     *
     * magLinear - a vector of costant linear magnitudes (aka amplitude ratio |H(f)|) at each respective frequency
     *  dB = 20 * log_10(mag), mag = pow(10, dB/20)
     *  Examples:
     *      mag = 1.0 -> 0 dB (no change)
     *      mag = 0.5 -> ~-6.02 boost cut
     *      mag = 2.0 -> ~ ~6.02 dB boost
     */
    void getFrequencyResponse(const std::vector<double>& freqs,
        std::vector<double>& magLinear) const;

    // For I/O Volume Meters
    float getInputPeakLinear(int ch) const 
    { 
        return inputPeak[juce::jlimit(0, 1, ch)].load(); 
    }

    float getOutputPeakLinear(int ch) const 
    { 
        return outputPeak[juce::jlimit(0, 1, ch)].load(); 
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static int  numStagesForSlopeIndex(int slopeIndex);
    static float readParam(const juce::AudioProcessorValueTreeState& s, const juce::String& id);

    // Only rebuild changed filters on the audio thread
    struct DirtyFlags
    {
        std::atomic<bool> hpf{ true };
        std::atomic<bool> lpf{ true };
        std::array<std::atomic<bool>, EqConstants::maxEqBands> peak{};
    } dirty;

    // For current parameter values, read once per process block
    struct BandSnapshot 
    { 
        bool enabled = false; 
        float freqHz = 1000.0f; 
        float q = 2.0f; 
        float gainDb = 0.0f; 
    };
    struct ChainSnapshot
    {
        float inGainDb = 0.0f;
        float outGainDb = 0.0f;
        
        bool hpfEnabled = false; 
        int hpfStages = 1; 
        float hpfFreqHz = 20.0f;
        int hpfIndex = 1;
        
        bool lpfEnabled = false; 
        int lpfStages = 1; 
        float lpfFreqHz = 20000.0f;
        int lpfIndex = 1;

        std::array<BandSnapshot, EqConstants::maxEqBands> bands{};
    } curSnap, lastSnap;

    void snapshotParameters(); // read apvts into curSnap
    void updateDirtyFilters(); // rebuilds coeffs if they're set as dirty

    // For coeffs of band peaks, hpf, and lpf EQ filters
    static IIRBiquadCoeffPtr makePeak(float sampleRate, float freqHz, float q, float gainDb);
    static IIRBiquadCoeffPtr makeHPF(float sampleRate, float freqHz, bool firstOrder);
    static IIRBiquadCoeffPtr makeLPF(float sampleRate, float freqHz, bool firstOrder);

    double currentSampleRate = 44100.0;
    juce::dsp::ProcessSpec lastSpec{};
    bool specValid = false;

    juce::dsp::Gain<float> inputGain;
    juce::dsp::Gain<float> outputGain;

    static constexpr int maxFilterStages = 4;

    // For HPF and LPF slope choices
    // Uses "cascades" - flatter slopes are chained/cascaded together multiple stages to get steeper slopes
    // More efficient to chain together the filters rather than have multiple different slope filters
    std::array<juce::dsp::IIR::Filter<float>, maxFilterStages> hpfL{};
    std::array<juce::dsp::IIR::Filter<float>, maxFilterStages> hpfR{};
    std::array<IIRBiquadCoeffPtr, maxFilterStages> hpfCoeffs{};

    std::array<juce::dsp::IIR::Filter<float>, maxFilterStages> lpfL{};
    std::array<juce::dsp::IIR::Filter<float>, maxFilterStages> lpfR{};
    std::array<IIRBiquadCoeffPtr, maxFilterStages> lpfCoeffs{};

    // Peaking bands per channel (left, right)
    std::array<juce::dsp::IIR::Filter<float>, EqConstants::maxEqBands> peaksL{};
    std::array<juce::dsp::IIR::Filter<float>, EqConstants::maxEqBands> peaksR{};
    std::array<IIRBiquadCoeffPtr, EqConstants::maxEqBands> peakCoeffs{};

    // Peak meters (updated each block)
    std::atomic<float> inputPeak[2]{ 0.0f, 0.0f };
    std::atomic<float> outputPeak[2]{ 0.0f, 0.0f };

    int hpfStageCount = 0;
    int lpfStageCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JuceEQAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
