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
 /**
     ImpulseResponsePlugin that loads an impulse response and applies it the audio stream.
     Additionally this has high and low pass filters to shape the sound.
 */

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

/** Destructor. */
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
/** Loads an impulse from binary audio file data i.e. not a block of raw floats.
    @see juce::Convolution::loadImpulseResponse
*/
void ImpulseResponsePlugin::loadImpulseResponse (const void* sourceData, size_t sourceDataSize,
    dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, size_t size,
    dsp::Convolution::Normalise requiresNormalisation)
{

    
    auto is = std::make_unique<MemoryInputStream> (sourceData, sourceDataSize, false);
    if (auto reader = std::unique_ptr<AudioFormatReader> (FlacAudioFormat().createReaderFor (is.release(), true)))
    {
        MemoryBlock fileDataMemoryBlock;
        if (auto writer = std::unique_ptr<AudioFormatWriter> (FlacAudioFormat().createWriterFor (new MemoryOutputStream (fileDataMemoryBlock, false),
            44100,
            reader->numChannels,
            24,
            {},
            0)))
        {
            writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples);
        }      
    }

    processorChain.get<convolutionIndex>().loadImpulseResponse (sourceData, sourceDataSize, isStereo, requiresTrimming, size, requiresNormalisation);

    
}

/** Loads an impulse from a file.
    @see juce::Convolution::loadImpulseResponse
*/
void ImpulseResponsePlugin::loadImpulseResponse (const File& fileImpulseResponse, size_t sizeInBytes, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming)
{
    MemoryBlock fileDataMemoryBlock;
    fileImpulseResponse.loadFileAsData (fileDataMemoryBlock);

    loadImpulseResponse (fileDataMemoryBlock.getData(), fileDataMemoryBlock.getSize(), isStereo, requiresTrimming, sizeInBytes, dsp::Convolution::Normalise::no);
}

/** Loads an impulse from an AudioBuffer<float>.
    @see juce::Convolution::loadImpulseResponse
*/
void ImpulseResponsePlugin::loadImpulseResponse (AudioBuffer<float>&& bufferImpulseResponse, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, dsp::Convolution::Normalise requiresNormalisation)
{
    juce::MemoryBlock fileDataMemoryBlock;

    if (auto writer = std::unique_ptr<AudioFormatWriter> (FlacAudioFormat().createWriterFor (new MemoryOutputStream (fileDataMemoryBlock, false),
        44100,
        bufferImpulseResponse.getNumChannels(),
        24,
        {},
        0)))
    {
        writer->writeFromAudioSampleBuffer (bufferImpulseResponse, 0, bufferImpulseResponse.getNumSamples());
    }

    state.setProperty (IDs::irFileData, juce::var (std::move (fileDataMemoryBlock)), getUndoManager());

    processorChain.get<convolutionIndex>().loadImpulseResponse (std::move (bufferImpulseResponse), sampleRate, isStereo, requiresTrimming, requiresNormalisation);
}


//==============================================================================
/** @internal */
juce::String ImpulseResponsePlugin::getName() { return getPluginName(); }
/** @internal */
juce::String ImpulseResponsePlugin::getPluginType() { return xmlTypeName; }
/** @internal */
bool ImpulseResponsePlugin::needsConstantBufferSize() { return false; }
/** @internal */
juce::String ImpulseResponsePlugin::getSelectableDescription() { return getName(); }

/** @internal */
double ImpulseResponsePlugin::getLatencySeconds()
{
    return processorChain.get<convolutionIndex>().getLatency() / sampleRate;
}

/** @internal */
void ImpulseResponsePlugin::initialise (const PluginInitialisationInfo& info)
{
    dsp::ProcessSpec processSpec;
    processSpec.sampleRate = info.sampleRate;
    processSpec.maximumBlockSize = (uint32_t) info.blockSizeSamples;
    processSpec.numChannels = 2;
    processorChain.prepare (processSpec);
}

/** @internal */
void ImpulseResponsePlugin::deinitialise()
{}

/** @internal */
void ImpulseResponsePlugin::reset()
{
    processorChain.reset();
}

/** @internal */
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

/** @internal */
void ImpulseResponsePlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    CachedValue <float>* cvsFloat[] = { &preGainValue, &postGainValue, &highPassCutoffValue, &lowPassCutoffValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}



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
            loadImpulseResponse (std::move (loadIRBuffer),
                reader->numChannels > 1 ? dsp::Convolution::Stereo::yes : dsp::Convolution::Stereo::no,
                dsp::Convolution::Trim::no, dsp::Convolution::Normalise::no);
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