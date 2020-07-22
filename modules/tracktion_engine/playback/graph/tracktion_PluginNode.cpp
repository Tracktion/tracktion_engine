/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_engine
{

namespace
{
    static bool shouldUseFineGrainAutomation (Plugin& p)
    {
        if (! p.isAutomationNeeded())
            return false;

        if (p.engine.getPluginManager().canUseFineGrainAutomation)
            return p.engine.getPluginManager().canUseFineGrainAutomation (p);

        return true;
    }
}

PluginNode::PluginNode (std::unique_ptr<Node> inputNode,
                        tracktion_engine::Plugin::Ptr pluginToProcess,
                        double sampleRateToUse, int blockSizeToUse,
                        std::shared_ptr<InputProvider> contextProvider)
    : input (std::move (inputNode)),
      plugin (std::move (pluginToProcess)),
      audioRenderContextProvider (std::move (contextProvider))
{
    jassert (input != nullptr);
    jassert (plugin != nullptr);
    initialisePlugin (sampleRateToUse, blockSizeToUse);
}

PluginNode::PluginNode (std::unique_ptr<Node> inputNode,
                        tracktion_engine::Plugin::Ptr pluginToProcess,
                        double sampleRateToUse, int blockSizeToUse,
                        const TrackMuteState* trackMuteStateToUse,
                        tracktion_graph::PlayHeadState& playHeadStateToUse, bool rendering)
    : input (std::move (inputNode)),
      plugin (std::move (pluginToProcess)),
      trackMuteState (trackMuteStateToUse),
      playHeadState (&playHeadStateToUse),
      isRendering (rendering)
{
    jassert (input != nullptr);
    jassert (plugin != nullptr);
    initialisePlugin (sampleRateToUse, blockSizeToUse);
}

PluginNode::~PluginNode()
{
    if (isInitialised && ! plugin->baseClassNeedsInitialising())
        plugin->baseClassDeinitialise();
}

//==============================================================================
tracktion_graph::NodeProperties PluginNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    const auto latencyNumSamples = juce::roundToInt (plugin->getLatencySeconds() * sampleRate);

    props.numberOfChannels = juce::jmax (1, props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (props.numberOfChannels));
    props.hasAudio = props.hasAudio || plugin->producesAudioWhenNoAudioInput();
    props.hasMidi  = props.hasMidi || plugin->takesMidiInput();
    props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);
    props.nodeID = (size_t) plugin->itemID.getRawID();

    return props;
}

void PluginNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    juce::ignoreUnused (info);
    jassert (sampleRate == info.sampleRate);
    
    if (shouldUseFineGrainAutomation (*plugin))
        subBlockSizeToUse = std::max (128, 128 * juce::roundToInt (info.sampleRate / 44100.0));
    
    canProcessBypassed = dynamic_cast<ExternalPlugin*> (plugin.get()) != nullptr
                            && plugin->getLatencySeconds() > 0;
}

void PluginNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    plugin->prepareForNextBlock (tracktion_graph::sampleToTime (referenceSampleRange.getStart(), sampleRate));
}

void PluginNode::process (const ProcessContext& pc)
{
    auto inputBuffers = input->getProcessedOutput();
    auto& inputAudioBlock = inputBuffers.audio;
    
    auto& outputBuffers = pc.buffers;
    auto& outputAudioBlock = outputBuffers.audio;
    jassert (inputAudioBlock.getNumSamples() == outputAudioBlock.getNumSamples());

    // Copy the inputs to the outputs, then process using the
    // output buffers as that will be the correct size
    {
        const size_t numInputChannelsToCopy = std::min (inputAudioBlock.getNumChannels(), outputAudioBlock.getNumChannels());
        
        if (numInputChannelsToCopy > 0)
            outputAudioBlock.copyFrom (inputAudioBlock.getSubsetChannelBlock (0, numInputChannelsToCopy));
    }
    
    // Init block
    const size_t subBlockSize = subBlockSizeToUse == -1 ? inputAudioBlock.getNumSamples()
                                                        : (size_t) subBlockSizeToUse;
    
    size_t numSamplesDone = 0;
    size_t numSamplesLeft = inputAudioBlock.getNumSamples();
    
    midiMessageArray.clear();
    bool shouldProcessPlugin = canProcessBypassed || plugin->isEnabled();
    bool isAllNotesOff = inputBuffers.midi.isAllNotesOff;
    
    if (playHeadState != nullptr && playHeadState->didPlayheadJump())
        isAllNotesOff = true;
    
    if (trackMuteState != nullptr)
    {
        if (! trackMuteState->shouldTrackContentsBeProcessed())
        {
            shouldProcessPlugin = shouldProcessPlugin && trackMuteState->shouldTrackBeAudible();
        
            if (trackMuteState->wasJustMuted())
                isAllNotesOff = true;
        }
    }
    
    auto inputMidiIter = inputBuffers.midi.begin();

    // Process in blocks
    for (;;)
    {
        size_t numSamplesThisBlock = std::min (subBlockSize, numSamplesLeft);

        auto outputAudioBuffer = tracktion_graph::test_utilities::createAudioBuffer (outputAudioBlock.getSubBlock (numSamplesDone, numSamplesThisBlock));
        
        const auto subBlockTimeRange = tracktion_graph::sampleToTime (juce::Range<size_t>::withStartAndLength (numSamplesDone, numSamplesThisBlock), sampleRate);
        midiMessageArray.copyFrom (inputBuffers.midi);
        
        midiMessageArray.clear();
        midiMessageArray.isAllNotesOff = isAllNotesOff;
        
        for (; inputMidiIter != inputBuffers.midi.end(); ++inputMidiIter)
        {
            const auto timestamp = inputMidiIter->getTimeStamp();
            
            if (timestamp >= subBlockTimeRange.getEnd())
                break;
            
            midiMessageArray.addMidiMessage (*inputMidiIter,
                                             timestamp - subBlockTimeRange.getStart(),
                                             inputMidiIter->mpeSourceID);
        }
        
        // Process the plugin
        //TODO: If a plugin is disabled we should probably apply our own latency to the plugin
        if (shouldProcessPlugin)
            plugin->applyToBufferWithAutomation (getPluginRenderContext (pc.referenceSampleRange.getStart() + (int64_t) numSamplesDone, outputAudioBuffer));

        // Then copy the buffers to the outputs
        outputBuffers.midi.mergeFrom (midiMessageArray);

        numSamplesDone += numSamplesThisBlock;
        numSamplesLeft -= numSamplesThisBlock;
        
        if (numSamplesLeft == 0)
            break;

        isAllNotesOff = false;
    }
}

//==============================================================================
void PluginNode::initialisePlugin (double sampleRateToUse, int blockSizeToUse)
{
    tracktion_engine::PlayHead enginePlayHead;
    tracktion_engine::PlaybackInitialisationInfo teInfo =
    {
        0.0, sampleRateToUse, blockSizeToUse, {}, enginePlayHead
    };
    
    plugin->baseClassInitialise (teInfo);
    isInitialised = true;

    sampleRate = sampleRateToUse;
}

PluginRenderContext PluginNode::getPluginRenderContext (int64_t referenceSamplePosition, juce::AudioBuffer<float>& destBuffer)
{
    if (audioRenderContextProvider != nullptr)
    {
        tracktion_engine::PluginRenderContext rc (audioRenderContextProvider->getContext());
        rc.destBuffer = &destBuffer;
        rc.bufferStartSample = 0;
        rc.bufferNumSamples = destBuffer.getNumSamples();
        rc.bufferForMidiMessages = &midiMessageArray;
        rc.midiBufferOffset = 0.0;
        
        return rc;
    }

    jassert (playHeadState != nullptr);
    auto& playHead = playHeadState->playHead;
    
    return { &destBuffer,
             juce::AudioChannelSet::canonicalChannelSet (destBuffer.getNumChannels()),
             0, destBuffer.getNumSamples(),
             &midiMessageArray, 0.0,
             tracktion_graph::sampleToTime (playHead.referenceSamplePositionToTimelinePosition (referenceSamplePosition), sampleRate),
             playHead.isPlaying(), playHead.isUserDragging(), isRendering, canProcessBypassed };
}

}
