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

//==============================================================================
ImpulseResponsePlugin::ImpulseResponsePlugin (PluginCreationInfo info)
    : Plugin (info)
{
    auto um = getUndoManager();
    preGainValue.referTo (state, "preGain", um, 1.0f);
    postGainValue.referTo (state, "postGain", um, 1.0f);
    highPassCutoffValue.referTo (state, "highPassCutoff", um, 0.1f);
    lowPassCutoffValue.referTo (state, "lowPassCutoff", um, 20000.0f);

    // Initialises parameter and attaches to value
    preGainParam = addParam ("preGain", TRANS ("Pre Gain"), { 0.1f, 20.0f });
    preGainParam->attachToCurrentValue (preGainValue);

    postGainParam = addParam ("postGain", TRANS ("Post Gain"), { 0.1f, 20.0f });
    postGainParam->attachToCurrentValue (postGainValue);

    highPassCutoffParam = addParam ("highPassCutoff", TRANS ("High Pass Filter Cutoff"), { 0.1f, 20000.0f });
    highPassCutoffParam->attachToCurrentValue (highPassCutoffValue);

    lowPassCutoffParam = addParam ("lowPassCutoff", TRANS ("Low Pass Filter Cutoff"), { 0.1f, 20000.0f });
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
    preGain.setGainLinear (preGainParam->getCurrentValue());

    auto hpf = processorChain.get<HPFIndex>().state;
    *hpf = dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, highPassCutoffParam->getCurrentValue());

    auto& lpf = processorChain.get<LPFIndex>().state;
    *lpf = dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lowPassCutoffParam->getCurrentValue());

    auto& postGain = processorChain.get<postGainIndex>();
    postGain.setGainLinear (postGainParam->getCurrentValue());

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
                                                                        dsp::Convolution::Trim::no,
                                                                        dsp::Convolution::Normalise::no);
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
