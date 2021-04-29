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

    // Assume a stereo output here to corretly initialise plugins
    // We might need to modify this to return a number of channels passed as an argument if there are differences with mono renders
    props.numberOfChannels = juce::jmax (2, props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (std::max (2, props.numberOfChannels)));
    props.hasAudio = props.hasAudio || plugin->producesAudioWhenNoAudioInput();
    props.hasMidi  = props.hasMidi || plugin->takesMidiInput();
    props.latencyNumSamples = props.latencyNumSamples + latencyNumSamples;
    props.nodeID = (size_t) plugin->itemID.getRawID();

    return props;
}

void PluginNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    juce::ignoreUnused (info);
    jassert (sampleRate == info.sampleRate);
    
    auto props = getNodeProperties();

    if (props.latencyNumSamples > 0)
        automationAdjustmentTime = -tracktion_graph::sampleToTime (props.latencyNumSamples, sampleRate);
    
    if (shouldUseFineGrainAutomation (*plugin))
        subBlockSizeToUse = std::max (128, 128 * juce::roundToInt (info.sampleRate / 44100.0));
    
    canProcessBypassed = dynamic_cast<ExternalPlugin*> (plugin.get()) != nullptr
                            && latencyNumSamples > 0;
    
    if (canProcessBypassed)
    {
        replaceLatencyProcessorIfPossible (info.rootNodeToReplace);
        
        if (! latencyProcessor)
        {
            latencyProcessor = std::make_shared<tracktion_graph::LatencyProcessor>();
            latencyProcessor->setLatencyNumSamples (latencyNumSamples);
            latencyProcessor->prepareToPlay (info.sampleRate, info.blockSize, props.numberOfChannels);
        }
    }
}

void PluginNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    plugin->prepareForNextBlock (tracktion_graph::sampleToTime (referenceSampleRange.getStart(), sampleRate));
}

void PluginNode::process (ProcessContext& pc)
{
    auto inputBuffers = input->getProcessedOutput();
    auto& inputAudioBlock = inputBuffers.audio;
    
    auto& outputBuffers = pc.buffers;
    auto outputAudioView = outputBuffers.audio;
    jassert (inputAudioBlock.getNumFrames() == outputAudioView.getNumFrames());

    const auto numInputChannelsToCopy = std::min (inputAudioBlock.getNumChannels(),
                                                  outputAudioView.getNumChannels());

    if (latencyProcessor)
    {
        if (numInputChannelsToCopy > 0)
            latencyProcessor->writeAudio (inputAudioBlock.getFirstChannels (numInputChannelsToCopy));
        
        latencyProcessor->writeMIDI (inputBuffers.midi);
    }

    // Copy the inputs to the outputs, then process using the
    // output buffers as that will be the correct size
    if (numInputChannelsToCopy > 0)
        copy (outputAudioView.getFirstChannels (numInputChannelsToCopy),
              inputAudioBlock.getFirstChannels (numInputChannelsToCopy));
    
    // Init block
    auto subBlockSize = subBlockSizeToUse < 0 ? inputAudioBlock.getNumFrames()
                                              : (choc::buffer::FrameCount) subBlockSizeToUse;
    
    choc::buffer::FrameCount numSamplesDone = 0;
    auto numSamplesLeft = inputAudioBlock.getNumFrames();
    
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
        auto numSamplesThisBlock = std::min (subBlockSize, numSamplesLeft);

        auto outputAudioBuffer = tracktion_graph::toAudioBuffer (outputAudioView.getFrameRange (tracktion_graph::frameRangeWithStartAndLength (numSamplesDone, numSamplesThisBlock)));
        
        const auto subBlockTimeRange = tracktion_graph::sampleToTime (juce::Range<size_t>::withStartAndLength (numSamplesDone, numSamplesThisBlock), sampleRate);
        
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

    // If the plugin was bypassed, use the delayed audio
    if (latencyProcessor)
    {
        // A slightly better approach would be to crossfade between the processed and latency block to minimise any discrepancies
        if (plugin->isEnabled())
        {
            auto numSamples = (int) inputAudioBlock.getNumFrames();
            latencyProcessor->clearAudio (numSamples);
            latencyProcessor->clearMIDI (numSamples);
        }
        else
        {
            outputBuffers.midi.clear();
            outputBuffers.audio.clear();

            // If no inputs have been added to the fifo, there won't be any samples available so skip
            if (numInputChannelsToCopy > 0)
                latencyProcessor->readAudio (outputAudioView);
            
            latencyProcessor->readMIDI (outputBuffers.midi, (int) inputAudioBlock.getNumFrames());
        }
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
    latencyNumSamples = juce::roundToInt (plugin->getLatencySeconds() * sampleRate);
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
             tracktion_graph::sampleToTime (playHead.referenceSamplePositionToTimelinePosition (referenceSamplePosition), sampleRate) + automationAdjustmentTime,
             playHead.isPlaying(), playHead.isUserDragging(), isRendering, canProcessBypassed };
}

void PluginNode::replaceLatencyProcessorIfPossible (Node* rootNodeToReplace)
{
    if (rootNodeToReplace == nullptr)
        return;
    
    auto props = getNodeProperties();
    auto nodeIDToLookFor = props.nodeID;
    
    if (nodeIDToLookFor == 0)
        return;

    auto visitor = [this, nodeIDToLookFor, props] (Node& node)
    {
        if (auto other = dynamic_cast<PluginNode*> (&node))
        {
            if (other->getNodeProperties().nodeID == nodeIDToLookFor)
            {
                if (! other->latencyProcessor)
                    return;
                
                if (! latencyProcessor)
                {
                    if (other->latencyProcessor->hasConfiguration (latencyNumSamples, sampleRate, props.numberOfChannels))
                        latencyProcessor = other->latencyProcessor;

                    return;
                }

                if (latencyProcessor->hasSameConfigurationAs (*other->latencyProcessor))
                    latencyProcessor = other->latencyProcessor;
            }
        }
    };
    visitNodes (*rootNodeToReplace, visitor, true);
}

}
