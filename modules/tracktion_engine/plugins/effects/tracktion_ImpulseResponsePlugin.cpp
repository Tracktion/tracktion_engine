/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion_engine
{

static float linearToFreq (float lin)       { return 440.0f * std::pow (2.0f, (lin - 69) / 12.0f); };
static float freqToLinear (float freq)      { return 12.0f * std::log2 (freq / 440.0f) + 69.0f; };

//==============================================================================
ImpulseResponsePlugin::ImpulseResponsePlugin (PluginCreationInfo info)
    : Plugin (info)
{
    auto um = getUndoManager();
    preGainValue.referTo (state, IDs::preGain, um, 0.0f);
    postGainValue.referTo (state, IDs::postGain, um, 0.0f);
    highPassCutoffValue.referTo (state, IDs::highPassFrequency, um, freqToLinear (0.1f));
    lowPassCutoffValue.referTo (state, IDs::lowPassFrequency, um, freqToLinear (20000.0f));

    normalise.referTo (state, IDs::normalise, um, true);
    trimSilence.referTo (state, IDs::trimSilence, um, false);

    NormalisableRange volumeRange { -12.0f, 6.0f };
    volumeRange.setSkewForCentre (0.0f);

    NormalisableRange frequencyRange { freqToLinear (0.1f), freqToLinear (20000.0f) };

    // Initialises parameter and attaches to value
    preGainParam = addParam (IDs::preGain.toString(), TRANS ("Pre Gain"), volumeRange,
                             [] (float value)       { return String (value, 1) + " dB"; },
                             [] (const String& s)   { return s.getFloatValue(); });
    preGainParam->attachToCurrentValue (preGainValue);

    postGainParam = addParam (IDs::postGain.toString(), TRANS ("Post Gain"), volumeRange,                              
                              [] (float value)       { return String (value, 1) + " dB"; },
                              [] (const String& s)   { return s.getFloatValue(); });

    postGainParam->attachToCurrentValue (postGainValue);

    highPassCutoffParam = addParam (IDs::highPassFrequency.toString(), TRANS ("High Cut"), frequencyRange,
                                    [] (float value)       { return String (linearToFreq (value), 1) + " Hz"; },
                                    [] (const String& s)   { return freqToLinear (s.getFloatValue()); });

    highPassCutoffParam->attachToCurrentValue (highPassCutoffValue);


                              

    lowPassCutoffParam =  addParam (IDs::lowPassFrequency.toString(), TRANS ("Low Cut"), frequencyRange,
                                    [] (float value)       { return String (linearToFreq (value), 1) + " Hz"; },
                                    [] (const String& s)   { return freqToLinear (s.getFloatValue()); });



    lowPassCutoffParam->attachToCurrentValue (lowPassCutoffValue);

    loadImpulseResponseFromState();
}

ImpulseResponsePlugin::~ImpulseResponsePlugin()
{
    notifyListenersOfDeletion();
    preGainParam->detachFromCurrentValue();
    postGainParam->detachFromCurrentValue();
    highPassCutoffParam->detachFromCurrentValue();
    lowPassCutoffParam->detachFromCurrentValue();
}

const char* ImpulseResponsePlugin::getPluginName() { return NEEDS_TRANS ("Impulse Response"); }

//==============================================================================
bool ImpulseResponsePlugin::loadImpulseResponse (const void* sourceData, size_t sourceDataSize)
{
    auto is = std::make_unique<MemoryInputStream> (sourceData, sourceDataSize, false);
    auto& formatManager = engine.getAudioFileFormatManager().readFormatManager;
    
    if (auto reader = std::unique_ptr<AudioFormatReader> (formatManager.createReaderFor (std::move (is))))
    {
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

        return loadImpulseResponse (std::move (buffer), reader->sampleRate, (int) reader->bitsPerSample);
    }
    
    return false;
}

bool ImpulseResponsePlugin::loadImpulseResponse (const File& fileImpulseResponse)
{
    MemoryBlock fileDataMemoryBlock;
    
    if (fileImpulseResponse.loadFileAsData (fileDataMemoryBlock))
        return loadImpulseResponse (fileDataMemoryBlock.getData(), fileDataMemoryBlock.getSize());
    
    return false;
}

bool ImpulseResponsePlugin::loadImpulseResponse (AudioBuffer<float>&& bufferImpulseResponse,
                                                 double sampleRateToStore,
                                                 int bitDepthToStore)
{
    juce::MemoryBlock fileDataMemoryBlock;

    if (auto writer = std::unique_ptr<AudioFormatWriter> (FlacAudioFormat().createWriterFor (new MemoryOutputStream (fileDataMemoryBlock, false),
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
    dsp::ProcessSpec processSpec;
    processSpec.sampleRate = info.sampleRate;
    processSpec.maximumBlockSize = (uint32_t) info.blockSizeSamples;
    processSpec.numChannels = 2;
    processorChain.prepare (processSpec);
}

void ImpulseResponsePlugin::deinitialise()
{}

void ImpulseResponsePlugin::reset()
{
    processorChain.reset();
}

void ImpulseResponsePlugin::applyToBuffer (const PluginRenderContext& fc)
{
    auto& preGain = processorChain.get<preGainIndex>();
    preGain.setGainLinear (juce::Decibels::decibelsToGain (preGainParam->getCurrentValue()));

    auto hpf = processorChain.get<HPFIndex>().state;
    *hpf = dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, highPassCutoffParam->getCurrentValue());

    auto& lpf = processorChain.get<LPFIndex>().state;
    *lpf = dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lowPassCutoffParam->getCurrentValue());

    auto& postGain = processorChain.get<postGainIndex>();
    postGain.setGainLinear (juce::Decibels::decibelsToGain (postGainParam->getCurrentValue()));

    dsp::AudioBlock <float> inoutBlock (*fc.destBuffer);
    dsp::ProcessContextReplacing <float> context (inoutBlock);
    processorChain.process (context);
}

void ImpulseResponsePlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    CachedValue <float>* cvsFloat[] = { &preGainValue, &postGainValue, &highPassCutoffValue, &lowPassCutoffValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    
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
        auto is = std::make_unique<MemoryInputStream> (*irFileData, false);

        if (auto reader = std::unique_ptr<AudioFormatReader> (FlacAudioFormat().createReaderFor (is.release(), true)))
        {
            juce::AudioSampleBuffer loadIRBuffer ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&loadIRBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

            jassert (reader->numChannels > 0);
            processorChain.get<convolutionIndex>().loadImpulseResponse (std::move (loadIRBuffer),
                                                                        reader->sampleRate,
                                                                        reader->numChannels > 1 ? dsp::Convolution::Stereo::yes : dsp::Convolution::Stereo::no,
                                                                        trimSilence.get() ? dsp::Convolution::Trim::yes : dsp::Convolution::Trim::no,
                                                                        normalise.get() ? dsp::Convolution::Normalise::yes : dsp::Convolution::Normalise::no);
        }
    }
}

void ImpulseResponsePlugin::valueTreePropertyChanged (ValueTree& v, const juce::Identifier& id)
{
    if (v == state && id == IDs::irFileData)
        loadImpulseResponseFromState();
    else
        Plugin::valueTreePropertyChanged (v, id);
}

}
