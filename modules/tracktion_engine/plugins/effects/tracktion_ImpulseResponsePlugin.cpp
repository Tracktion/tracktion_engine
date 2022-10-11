/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace engine
{

//==============================================================================
ImpulseResponsePlugin::ImpulseResponsePlugin (PluginCreationInfo info)
    : Plugin (info)
{
    auto um = getUndoManager();

    name.referTo (state, IDs::name, um);
    
    const juce::NormalisableRange frequencyRange { frequencyToMidiNote (10.0f), frequencyToMidiNote (20'000.0f) };

    highPassCutoffValue.referTo (state, IDs::highPassFrequency, um, frequencyRange.start);
    lowPassCutoffValue.referTo (state, IDs::lowPassFrequency, um, frequencyRange.end);
    gainValue.referTo (state, IDs::gain, um, 0.0f);
    mixValue.referTo (state, IDs::mix, um, 1.0f);
    qValue.referTo (state, IDs::filterSlope, um, 1.0f / juce::MathConstants<float>::sqrt2);

    normalise.referTo (state, IDs::normalise, um, true);
    trimSilence.referTo (state, IDs::trimSilence, um, false);

    juce::NormalisableRange volumeRange { -12.0f, 6.0f };
    volumeRange.setSkewForCentre (0.0f);

    // Initialises parameter and attaches to value
    gainParam = addParam (IDs::gain.toString(), TRANS ("Gain"), volumeRange,
                          [] (float value)             { return juce::Decibels::toString (value); },
                          [] (const juce::String& s)   { return s.getFloatValue(); });
    gainParam->attachToCurrentValue (gainValue);

    highPassCutoffParam = addParam (IDs::highPassMidiNoteNumber.toString(), TRANS ("Low Cut"), frequencyRange,
                                    [] (float value)             { return juce::String (midiNoteToFrequency (value), 1) + " Hz"; },
                                    [] (const juce::String& s)   { return frequencyToMidiNote (s.getFloatValue()); });
    highPassCutoffParam->attachToCurrentValue (highPassCutoffValue);

    lowPassCutoffParam =  addParam (IDs::lowPassMidiNoteNumber.toString(), TRANS ("High Cut"), frequencyRange,
                                    [] (float value)             { return juce::String (midiNoteToFrequency (value), 1) + " Hz"; },
                                    [] (const juce::String& s)   { return frequencyToMidiNote (s.getFloatValue()); });
    lowPassCutoffParam->attachToCurrentValue (lowPassCutoffValue);

    mixParam = addParam (IDs::mix.toString(), TRANS("Mix"), { 0.0f, 1.0f, 0.0f },
                         [] (float value)             { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; },
                         [] (const juce::String& s)   { return s.getFloatValue() / 100.0f; });
    mixParam->attachToCurrentValue (mixValue);

    filterQParam = addParam (IDs::filterQ.toString(), TRANS("Filter Q"), { 0.1f, 14.0f, 0.0f },
                             [] (float value)       { return juce::String (value); },
                             [] (const juce::String& s)   { return s.getFloatValue(); });
    filterQParam->attachToCurrentValue (qValue);

    loadImpulseResponseFromState();
}

ImpulseResponsePlugin::~ImpulseResponsePlugin()
{
    notifyListenersOfDeletion();
    gainParam->detachFromCurrentValue();
    highPassCutoffParam->detachFromCurrentValue();
    lowPassCutoffParam->detachFromCurrentValue();
    mixParam->detachFromCurrentValue();
    filterQParam->detachFromCurrentValue();
}

const char* ImpulseResponsePlugin::getPluginName() { return NEEDS_TRANS ("Impulse Response"); }

//==============================================================================
bool ImpulseResponsePlugin::loadImpulseResponse (const void* sourceData, size_t sourceDataSize)
{
    auto is = std::make_unique<juce::MemoryInputStream> (sourceData, sourceDataSize, false);
    auto& formatManager = engine.getAudioFileFormatManager().readFormatManager;
    
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (std::move (is))))
    {
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

        return loadImpulseResponse (std::move (buffer), reader->sampleRate, (int) reader->bitsPerSample);
    }
    
    return false;
}

bool ImpulseResponsePlugin::loadImpulseResponse (const juce::File& fileImpulseResponse)
{
    juce::MemoryBlock fileDataMemoryBlock;
    
    if (fileImpulseResponse.loadFileAsData (fileDataMemoryBlock))
        return loadImpulseResponse (fileDataMemoryBlock.getData(), fileDataMemoryBlock.getSize());
    
    return false;
}

bool ImpulseResponsePlugin::loadImpulseResponse (juce::AudioBuffer<float>&& bufferImpulseResponse,
                                                 double sampleRateToStore,
                                                 int bitDepthToStore)
{
    juce::MemoryBlock fileDataMemoryBlock;

    if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (juce::FlacAudioFormat()
                                                                    .createWriterFor (new juce::MemoryOutputStream (fileDataMemoryBlock, false),
                                                                                      sampleRateToStore,
                                                                                      (unsigned int) bufferImpulseResponse.getNumChannels(),
                                                                                      std::min (24, bitDepthToStore),
                                                                                      {},
                                                                                      0)))
    {
        if (writer->writeFromAudioSampleBuffer (bufferImpulseResponse, 0, bufferImpulseResponse.getNumSamples()))
        {
            writer.reset(); // Delete the writer to flush the block before moving the buffer on
            state.setProperty (IDs::irFileData, juce::var (std::move (fileDataMemoryBlock)), getUndoManager());
            return true;
        }
    }

    return false;
}


//==============================================================================
juce::String ImpulseResponsePlugin::getName()                   { return getPluginName(); }
juce::String ImpulseResponsePlugin::getShortName (int)          { return "IR"; }
juce::String ImpulseResponsePlugin::getPluginType()             { return xmlTypeName; }
bool ImpulseResponsePlugin::needsConstantBufferSize()           { return false; }
juce::String ImpulseResponsePlugin::getSelectableDescription()  { return getName(); }

double ImpulseResponsePlugin::getLatencySeconds()
{
    return processorChain.get<convolutionIndex>().getLatency() / sampleRate;
}

void ImpulseResponsePlugin::initialise (const PluginInitialisationInfo& info)
{
    juce::dsp::ProcessSpec processSpec;
    processSpec.sampleRate = info.sampleRate;
    processSpec.maximumBlockSize = (uint32_t) info.blockSizeSamples;
    processSpec.numChannels = 2;
    processorChain.prepare (processSpec);

    // Update smoothers
    lowFreqSmoother.setTargetValue (midiNoteToFrequency (lowPassCutoffParam->getCurrentValue()));
    highFreqSmoother.setTargetValue (midiNoteToFrequency (highPassCutoffParam->getCurrentValue()));
    gainSmoother.setTargetValue (gainParam->getCurrentValue());
    qSmoother.setTargetValue (filterQParam->getCurrentValue());

    const auto wetDry = getWetDryLevels (mixParam->getCurrentValue());
    wetGainSmoother.setTargetValue (wetDry.wet);
    dryGainSmoother.setTargetValue (wetDry.dry);
    
    const double smoothTime = 0.01;
    lowFreqSmoother.reset (info.sampleRate, smoothTime);
    highFreqSmoother.reset (info.sampleRate, smoothTime);
    gainSmoother.reset (info.sampleRate, smoothTime);
    qSmoother.reset (info.sampleRate, smoothTime);
    wetGainSmoother.reset (info.sampleRate, smoothTime);
    dryGainSmoother.reset (info.sampleRate, smoothTime);
}

void ImpulseResponsePlugin::deinitialise()
{}

void ImpulseResponsePlugin::reset()
{
    processorChain.reset();
}

void ImpulseResponsePlugin::applyToBuffer (const PluginRenderContext& fc)
{
    // Update smoothers
    lowFreqSmoother.setTargetValue (midiNoteToFrequency (lowPassCutoffParam->getCurrentValue()));
    highFreqSmoother.setTargetValue (midiNoteToFrequency (highPassCutoffParam->getCurrentValue()));
    gainSmoother.setTargetValue (gainParam->getCurrentValue());
    qSmoother.setTargetValue (filterQParam->getCurrentValue());

    const auto wetDryGain = getWetDryLevels (mixParam->getCurrentValue());
    wetGainSmoother.setTargetValue (wetDryGain.wet);
    dryGainSmoother.setTargetValue (wetDryGain.dry);

    // Update gains and filter params
    auto hpf = processorChain.get<HPFIndex>().state;
    auto& lpf = processorChain.get<LPFIndex>().state;
    auto& gain = processorChain.get<gainIndex>();
    
    AudioScratchBuffer dryBuffer (*fc.destBuffer);
    
    if (gainSmoother.isSmoothing() || lowFreqSmoother.isSmoothing() || highFreqSmoother.isSmoothing() || qSmoother.isSmoothing())
    {
        const int blockSize = 32;
        int numSamplesLeft = fc.bufferNumSamples;
        int numSamplesDone = 0;
        
        for (;;)
        {
            const int numThisTime = std::min (blockSize, numSamplesLeft);

            // Process sub-block
            auto inOutBlock = juce::dsp::AudioBlock<float> (*fc.destBuffer).getSubBlock (size_t (numSamplesDone + fc.bufferStartSample),
                                                                                         size_t (numThisTime));
            juce::dsp::ProcessContextReplacing <float> context (inOutBlock);
            processorChain.process (context);

            // Update params
            const auto qFactor = qSmoother.skip (numThisTime);
            *hpf = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, highFreqSmoother.skip (numThisTime), qFactor);
            *lpf = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lowFreqSmoother.skip (numThisTime), qFactor);
            gain.setGainLinear (juce::Decibels::decibelsToGain (gainSmoother.skip (numThisTime)));

            numSamplesDone += numThisTime;
            numSamplesLeft -= blockSize;
            
            if (numSamplesDone == fc.bufferNumSamples)
                break;
        }
    }
    else
    {
        // Update params
        const auto qFactor = qSmoother.getCurrentValue();
        *hpf = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, highFreqSmoother.getCurrentValue(), qFactor);
        *lpf = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lowFreqSmoother.getCurrentValue(), qFactor);
        gain.setGainLinear (juce::Decibels::decibelsToGain (gainSmoother.getCurrentValue()));

        juce::dsp::AudioBlock<float> inOutBlock (*fc.destBuffer);
        juce::dsp::ProcessContextReplacing <float> context (inOutBlock);
        processorChain.process (context);
    }
    
    const bool isMixed = wetGainSmoother.getCurrentValue() < 1.0f;
    jassert (fc.bufferStartSample == 0); // This assumes bufferStartSample is always 0 which should be the case
    wetGainSmoother.applyGain (*fc.destBuffer, fc.bufferNumSamples);
    dryGainSmoother.applyGain (dryBuffer.buffer, dryBuffer.buffer.getNumSamples());
    jassert (fc.bufferNumSamples == dryBuffer.buffer.getNumSamples());
    jassert (fc.destBuffer->getNumChannels() == dryBuffer.buffer.getNumChannels());

    if (isMixed)
    {
        for (int c = 0; c < fc.destBuffer->getNumChannels(); ++c)
            fc.destBuffer->addFrom (c, fc.bufferStartSample, dryBuffer.buffer,
                                    c, fc.bufferStartSample, fc.bufferNumSamples);
    }
}

void ImpulseResponsePlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[] = { &gainValue, &highPassCutoffValue, &lowPassCutoffValue, &mixValue, &qValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    state.setProperty (IDs::name, v[IDs::name], getUndoManager());
    
    if (auto irFileData = v.getProperty (IDs::irFileData).getBinaryData())
        state.setProperty (IDs::irFileData, juce::var (juce::MemoryBlock (*irFileData)), getUndoManager());

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

//==============================================================================
void ImpulseResponsePlugin::loadImpulseResponseFromState()
{
    if (auto irFileData = state.getProperty (IDs::irFileData).getBinaryData())
    {
        auto is = std::make_unique<juce::MemoryInputStream> (*irFileData, false);

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (juce::FlacAudioFormat()
                                                                        .createReaderFor (is.release(), true)))
        {
            juce::AudioBuffer<float> loadIRBuffer ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&loadIRBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

            jassert (reader->numChannels > 0);
            processorChain.get<convolutionIndex>().loadImpulseResponse (std::move (loadIRBuffer),
                                                                        reader->sampleRate,
                                                                        reader->numChannels > 1 ? juce::dsp::Convolution::Stereo::yes
                                                                                                : juce::dsp::Convolution::Stereo::no,
                                                                        trimSilence.get() ? juce::dsp::Convolution::Trim::yes
                                                                                          : juce::dsp::Convolution::Trim::no,
                                                                        normalise.get() ? juce::dsp::Convolution::Normalise::yes
                                                                                        : juce::dsp::Convolution::Normalise::no);
        }
    }
}

void ImpulseResponsePlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
    {
        if (id == IDs::irFileData || id == IDs::normalise || id == IDs::trimSilence)
            loadImpulseResponseFromState();
    }
    else
    {
        Plugin::valueTreePropertyChanged (v, id);
    }
}

}} // namespace tracktion { inline namespace engine
