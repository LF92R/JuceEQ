#include "PluginProcessor.h"
// DO NOT include PluginEditor.h 
class JuceEQAudioProcessorEditor;
extern juce::AudioProcessorEditor* createEQEditor(JuceEQAudioProcessor&);

using namespace EqConstants;

static juce::StringArray slopeChoices() { return { "6 dB", "12 dB", "24 dB", "48 dB" }; }
static constexpr int defaultSlopeIndex = 0; // Default to 6 dB

JuceEQAudioProcessor::JuceEQAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "PARAMS", createParameterLayout())
{

    // HPF and LPF base coeffs before first plugin window render
    auto initFilterCoeffs = new IIRBiquadCoeffs(1, 0, 0, 1, 0, 0);

    for (int i = 0; i < maxFilterStages; ++i)
    {
        hpfCoeffs[i] = initFilterCoeffs;
        lpfCoeffs[i] = initFilterCoeffs;
        hpfL[i].coefficients = initFilterCoeffs;
        hpfR[i].coefficients = initFilterCoeffs;
        lpfL[i].coefficients = initFilterCoeffs;
        lpfR[i].coefficients = initFilterCoeffs;
    }

    for (int b = 0; b < maxEqBands; ++b)
    {
        peakCoeffs[b] = initFilterCoeffs;
        peaksL[b].coefficients = initFilterCoeffs;
        peaksR[b].coefficients = initFilterCoeffs;
    }

    // Prevents zipping noises when moving I/O faders
    inputGain.setRampDurationSeconds(0.02);
    outputGain.setRampDurationSeconds(0.02);
}

juce::AudioProcessorValueTreeState::ParameterLayout JuceEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // For I/O gain 
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "inGain", "Input", juce::NormalisableRange<float>(-60.0f, 10.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outGain", "Output", juce::NormalisableRange<float>(-60.0f, 10.0f, 0.01f), 0.0f));

    // HPF 
    params.push_back(std::make_unique<juce::AudioParameterBool>("hpfEnabled", "HPF Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "hpfFreq", "HPF Freq", juce::NormalisableRange<float>(minEqFreq, maxEqFreq, 0.01f, 0.5f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "hpfSlope", "HPF Slope", slopeChoices(), defaultSlopeIndex));

    // LPF
    params.push_back(std::make_unique<juce::AudioParameterBool>("lpfEnabled", "LPF Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lpfFreq", "LPF Freq", juce::NormalisableRange<float>(minEqFreq, maxEqFreq, 0.01f, 0.5f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "lpfSlope", "LPF Slope", slopeChoices(), defaultSlopeIndex));

    // For peaking bands, from 1 to (at max) 8
    for (int i = 1; i <= maxEqBands; ++i)
    {
        const bool enabledDefault = (i <= 3); // Default 3 bands

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            eqBandParamType(i, "enabled"), "B" + juce::String(i) + " Enabled", enabledDefault));

        const float defaultFreq = juce::jmap<float>((float)i, 1.0f, (float)maxEqBands, 100.0f, 5000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            eqBandParamType(i, "freq"), "B" + juce::String(i) + " Freq",
            juce::NormalisableRange<float>(minEqFreq, maxEqFreq, 0.01f, 0.5f), defaultFreq));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            eqBandParamType(i, "gain"), "B" + juce::String(i) + " Gain",
            juce::NormalisableRange<float>(minEqGainDb, maxEqGainDb, 0.01f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            eqBandParamType(i, "q"), "B" + juce::String(i) + " Q",
            juce::NormalisableRange<float>(eqMinQ, eqMaxQ, 0.01f, 0.5f), 2.0f));
    }

    return { params.begin(), params.end() };
}

void JuceEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // For multi-channel (Gain) processing
    juce::dsp::ProcessSpec fullSpec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };

    // NOTE: IIR::Filter is mono-only. 
    juce::dsp::ProcessSpec monoSpec{ sampleRate, (juce::uint32)samplesPerBlock, 1 };

    inputGain.prepare(fullSpec);
    outputGain.prepare(fullSpec);

    // Treat every filter as if were mono - hence each having a left and right
    for (int s = 0; s < maxFilterStages; ++s)
    {
        hpfL[s].prepare(monoSpec);
        hpfR[s].prepare(monoSpec);
        lpfL[s].prepare(monoSpec);
        lpfR[s].prepare(monoSpec);
    }

    for (int b = 0; b < maxEqBands; ++b)
    {
        peaksL[b].prepare(monoSpec);
        peaksR[b].prepare(monoSpec);
    }

    specValid = true;

    // Force first-time coeff build
    dirty.hpf.store(true);
    dirty.lpf.store(true);
    for (auto& d : dirty.peak) d.store(true);

    lastSnap = {}; // reset baseline
    snapshotParameters();
    updateDirtyFilters();
}

// Checks if mono or stereo is being used
bool JuceEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();

    if (in != out) 
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void JuceEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals; // For effeciency - rounds down very small floats to 0 to reduce processing load

    snapshotParameters();
    updateDirtyFilters(); // Rebuild only what's different from the last process block

    // Audio buffer
    juce::dsp::AudioBlock<float> block(buffer);

    // For multichannel contexts
    juce::dsp::ProcessContextReplacing<float> chContext(block);

    // Input gain affects all channels prior to EQ filter and band application
    inputGain.setGainDecibels(curSnap.inGainDb);
    inputGain.process(chContext);

    // Seperates channels into their own mono channels to be processed 
    const int numCh = buffer.getNumChannels();
    auto ch0Block = block.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<float> ctxL(ch0Block);

    // For mono only cases, use just the right channel
    std::unique_ptr<juce::dsp::ProcessContextReplacing<float>> ctxRPtr;
    if (numCh > 1)
    {
        auto ch1Block = block.getSingleChannelBlock(1);
        ctxRPtr = std::make_unique<juce::dsp::ProcessContextReplacing<float>>(ch1Block);
    }

    // Helper for mono-only filters 
    auto processMonoStage = [&](juce::dsp::IIR::Filter<float>& fl,
        juce::dsp::IIR::Filter<float>& fr,
        int stages,
        bool enabled)
        {
            if (!enabled) return;
            for (int s = 0; s < stages; ++s)
            {
                fl.process(ctxL);
                if (ctxRPtr) fr.process(*ctxRPtr);
            }
        };

    auto processMonoBand = [&](int b)
        {
            if (!curSnap.bands[b].enabled) return;
            peaksL[b].process(ctxL);
            if (ctxRPtr) peaksR[b].process(*ctxRPtr);
        };

    // HPF cascade (mono)
    processMonoStage(hpfL[0], hpfR[0], curSnap.hpfStages, curSnap.hpfEnabled);

    // Peaking bands
    for (int b = 0; b < maxEqBands; ++b)
        processMonoBand(b);

    // LPF cascade (mono)
    processMonoStage(lpfL[0], lpfR[0], curSnap.lpfStages, curSnap.lpfEnabled);

    // Apply output gain to all channels 
    outputGain.setGainDecibels(curSnap.outGainDb);
    outputGain.process(chContext);
}

// Getter and setter for preset info
void JuceEQAudioProcessor::getStateInformation(juce::MemoryBlock& dataDest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dataDest);
}

void JuceEQAudioProcessor::setStateInformation(const void* xmlData, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(xmlData, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// For slope index
int JuceEQAudioProcessor::numStagesForSlopeIndex(int slopeIndex)
{
    if (slopeIndex <= 1) 
        return 1; // 6 or 12 dB -> 1 stage

    return 1 << (slopeIndex - 1);  // 24 -> 2 stages, 48 -> 4 stages
}

float JuceEQAudioProcessor::readParam(const juce::AudioProcessorValueTreeState& apvtState, const juce::String& id)
{
    if (auto* p = apvtState.getRawParameterValue(id)) 
        return p->load();

    return 0.0f;
}

void JuceEQAudioProcessor::snapshotParameters()
{
    ChainSnapshot snap;

    snap.inGainDb = readParam(apvts, "inGain");
    snap.outGainDb = readParam(apvts, "outGain");

    snap.hpfIndex = (int)readParam(apvts, "hpfSlope");
    snap.hpfEnabled = readParam(apvts, "hpfEnabled") > 0.5f;
    snap.hpfFreqHz = readParam(apvts, "hpfFreq");
    snap.hpfStages = numStagesForSlopeIndex(snap.hpfIndex);

    snap.lpfIndex = (int)readParam(apvts, "lpfSlope");
    snap.lpfEnabled = readParam(apvts, "lpfEnabled") > 0.5f;
    snap.lpfFreqHz = readParam(apvts, "lpfFreq");
    snap.lpfStages = numStagesForSlopeIndex(snap.lpfIndex);

    // For EQ bands
    for (int b = 0; b < maxEqBands; ++b)
    {
        const int i = b + 1;
        auto& band = snap.bands[b];
        band.enabled = readParam(apvts, eqBandParamType(i, "enabled")) > 0.5f;
        band.freqHz = readParam(apvts, eqBandParamType(i, "freq"));
        band.q = readParam(apvts, eqBandParamType(i, "q"));
        band.gainDb = readParam(apvts, eqBandParamType(i, "gain"));
    }

    // Snapshot detection - checks for parameter changes
    if (snap.hpfEnabled != lastSnap.hpfEnabled || snap.hpfFreqHz != lastSnap.hpfFreqHz || snap.hpfStages != lastSnap.hpfStages || snap.hpfIndex != lastSnap.hpfIndex)
        dirty.hpf.store(true);
    if (snap.lpfEnabled != lastSnap.lpfEnabled || snap.lpfFreqHz != lastSnap.lpfFreqHz || snap.lpfStages != lastSnap.lpfStages || snap.lpfIndex != lastSnap.lpfIndex)
        dirty.lpf.store(true);

    for (int b = 0; b < maxEqBands; ++b)
    {
        const auto& a = snap.bands[b];
        const auto& z = lastSnap.bands[b];
        if (a.enabled != z.enabled || a.freqHz != z.freqHz || a.q != z.q || a.gainDb != z.gainDb)
            dirty.peak[b].store(true);
    }

    curSnap = snap;
    lastSnap = snap;
}

JuceEQAudioProcessor::IIRBiquadCoeffPtr JuceEQAudioProcessor::makePeak(float sampleRate, float freqHz, float q, float gainDb)
{
    const float g = juce::Decibels::decibelsToGain(juce::jlimit(minEqGainDb, maxEqGainDb, gainDb));

    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
        juce::jlimit(minEqFreq, maxEqFreq, freqHz), juce::jlimit(eqMinQ, eqMaxQ, q), g);
}

JuceEQAudioProcessor::IIRBiquadCoeffPtr JuceEQAudioProcessor::makeHPF(float sampleRate, float freqHz, bool firstOrder)
{
    const float f = juce::jlimit(minEqFreq, maxEqFreq, freqHz);

    return firstOrder ? IIRBiquadCoeffs::makeFirstOrderHighPass(sampleRate, f)
        : IIRBiquadCoeffs::makeHighPass(sampleRate, f);
}

JuceEQAudioProcessor::IIRBiquadCoeffPtr JuceEQAudioProcessor::makeLPF(float sampleRate, float freqHz, bool firstOrder)
{
    const float f = juce::jlimit(minEqFreq, maxEqFreq, freqHz);

    return firstOrder ? IIRBiquadCoeffs::makeFirstOrderLowPass(sampleRate, f)
        : IIRBiquadCoeffs::makeLowPass(sampleRate, f);
}

void JuceEQAudioProcessor::updateDirtyFilters()
{
    const auto sampleRate = (float)currentSampleRate;

    if (dirty.hpf.exchange(false))
    {
        const bool firstOrder = (curSnap.hpfIndex == 0); // 6 dB -> 1st order
        auto c = makeHPF(sampleRate, curSnap.hpfFreqHz, firstOrder);
        
        for (int i = 0; i < maxFilterStages; ++i) 
            hpfCoeffs[i] = c;
        
        for (int i = 0; i < maxFilterStages; ++i) 
        { 
            hpfL[i].coefficients = hpfCoeffs[i]; 
            hpfR[i].coefficients = hpfCoeffs[i]; 
        }

        hpfStageCount = curSnap.hpfStages;
    }

    if (dirty.lpf.exchange(false))
    {
        const bool firstOrder = (curSnap.lpfIndex == 0); // 6 dB -> 1st order
        auto c = makeLPF(sampleRate, curSnap.lpfFreqHz, firstOrder);
        
        for (int i = 0; i < maxFilterStages; ++i) 
            lpfCoeffs[i] = c;
        
        for (int i = 0; i < maxFilterStages; ++i) 
        { 
            lpfL[i].coefficients = lpfCoeffs[i]; 
            lpfR[i].coefficients = lpfCoeffs[i]; 
        }

        lpfStageCount = curSnap.lpfStages;
    }

    // EQ bands
    for (int b = 0; b < maxEqBands; ++b)
    {
        if (!dirty.peak[b].exchange(false)) continue;

        if (curSnap.bands[b].enabled)
            peakCoeffs[b] = makePeak(sampleRate, curSnap.bands[b].freqHz, curSnap.bands[b].q, curSnap.bands[b].gainDb);
        else
            peakCoeffs[b] = IIRBiquadCoeffPtr();

        peaksL[b].coefficients = peakCoeffs[b];
        peaksR[b].coefficients = peakCoeffs[b];
    }
}

void JuceEQAudioProcessor::getFrequencyResponse(const std::vector<double>& freqs,
    std::vector<double>& mags) const
{
    jassert(mags.size() == freqs.size());

    // For local copies of the shared_ptr arrays
    // Safer in case of thread swaps during the for loop
    auto locHpfC = hpfCoeffs;
    auto locLpfC = lpfCoeffs;
    auto locPeakC = peakCoeffs;

    const bool locHpfEnabled = curSnap.hpfEnabled;
    const int  locHpfCount = hpfStageCount;
    const bool locLpfEnabled = curSnap.lpfEnabled;
    const int  locLpfCount = lpfStageCount;

    for (size_t i = 0; i < freqs.size(); ++i)
    {
        const double newFreq = juce::jlimit<double>(minEqFreq, maxEqFreq, freqs[i]);
        double H = 1.0;

        if (locHpfEnabled) {
            for (int s = 0; s < locHpfCount; ++s)
            {
                if (locHpfC[s] != nullptr)
                    H *= locHpfC[s]->getMagnitudeForFrequency(newFreq, currentSampleRate);
            }
        }
               
        for (int b = 0; b < maxEqBands; ++b)
            if (locPeakC[b] != nullptr) 
                H *= locPeakC[b]->getMagnitudeForFrequency(newFreq, currentSampleRate);

        if (locLpfEnabled) {
            for (int s = 0; s < locLpfCount; ++s)
            {
                if (locLpfC[s] != nullptr)
                    H *= locLpfC[s]->getMagnitudeForFrequency(newFreq, currentSampleRate);
            }
        }

        mags[i] = H;
    }
}

juce::AudioProcessorEditor* JuceEQAudioProcessor::createEditor()
{
    return createEQEditor(*this);
}

// JUCE entry point - called to construct a processor instance
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuceEQAudioProcessor();
}
