/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine {

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

//==============================================================================
PluginNode::PluginNode (std::unique_ptr<Node> inputNode,
                        tracktion::engine::Plugin::Ptr pluginToProcess,
                        double sampleRateToUse, int blockSizeToUse,
                        const TrackMuteState* trackMuteStateToUse,
                        ProcessState& processStateToUse,
                        bool rendering, bool canBalanceLatency,
                        int maxNumChannelsToUse)
    : TracktionEngineNode (processStateToUse),
      input (std::move (inputNode)),
      plugin (std::move (pluginToProcess)),
      trackMuteState (trackMuteStateToUse),
      playHeadState (processStateToUse.playHeadState),
      isRendering (rendering),
      maxNumChannels (maxNumChannelsToUse),
      balanceLatency (canBalanceLatency)
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
tracktion::graph::NodeProperties PluginNode::getNodeProperties()
{
    if (cachedNodeProperties)
        return *cachedNodeProperties;

    auto props = input->getNodeProperties();

    // Use the actual input channel count — channel conversion (e.g. mono→stereo) is handled
    // upstream by createNodeForPlugin inserting a ChannelRemappingNode when needed
    props.numberOfChannels = std::max (props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (props.numberOfChannels));

    // Ensure audio plugins always get at least their declared minimum channel count,
    // even when the input node reports 0 channels (e.g. unconnected rack plugin).
    // Only truly MIDI-only plugins (no audio in, no audio out) are excluded.
    if (plugin->takesAudioInput() || plugin->producesAudioWhenNoAudioInput())
    {
        const auto pluginBusses = plugin->getBusses();

        if (! pluginBusses.inputs.empty())
            props.numberOfChannels = std::max (props.numberOfChannels,
                                               pluginBusses.inputs.front().getNumChannels());

        if (! pluginBusses.outputs.empty())
        {
            int totalOutputChans = 0;

            for (const auto& bus : pluginBusses.outputs)
                totalOutputChans += bus.getNumChannels();

            props.numberOfChannels = std::max (props.numberOfChannels, totalOutputChans);
        }
    }

    if (maxNumChannels > 0)
        props.numberOfChannels = std::min (props.numberOfChannels, maxNumChannels);

    props.hasAudio = props.hasAudio || plugin->producesAudioWhenNoAudioInput();
    props.hasMidi  = props.hasMidi || plugin->takesMidiInput();
    props.latencyNumSamples = std::max (0, props.latencyNumSamples + latencyNumSamples);

    if (props.nodeID != 0)
        hash_combine (props.nodeID, (size_t) plugin->itemID.getRawID());
    else
        props.nodeID = (size_t) plugin->itemID.getRawID();

    if (isPrepared)
        cachedNodeProperties = props;

    return props;
}

void PluginNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    juce::ignoreUnused (info);
    jassert (sampleRate == info.sampleRate);
    jassert (! isPrepared); // Is this being called multiple times?

    auto props = getNodeProperties();

    if (props.latencyNumSamples > 0)
        automationAdjustmentTime = TimeDuration::fromSamples (-props.latencyNumSamples, sampleRate);

    if (shouldUseFineGrainAutomation (*plugin))
        subBlockSizeToUse = std::max (128, 128 * juce::roundToInt (info.sampleRate / 44100.0));

    canProcessBypassed = balanceLatency
                            && dynamic_cast<ExternalPlugin*> (plugin.get()) != nullptr
                            && latencyNumSamples > 0;

    if (canProcessBypassed)
    {
        replaceLatencyProcessorIfPossible (info.nodeGraphToReplace);

        if (! latencyProcessor)
        {
            latencyProcessor = std::make_shared<tracktion::graph::LatencyProcessor>();
            latencyProcessor->setLatencyNumSamples (latencyNumSamples);
            latencyProcessor->prepareToPlay (info.sampleRate, info.blockSize, props.numberOfChannels);
        }
    }

    isPrepared = true;

    if (info.enableNodeMemorySharing && input->numOutputNodes == 1)
    {
        const auto inputNumChannels = input->getNodeProperties().numberOfChannels;
        const auto desiredNumChannels = props.numberOfChannels;

        if (inputNumChannels >= desiredNumChannels)
        {
            canUseSourceBuffers = true;
            setOptimisations ({ tracktion::graph::ClearBuffers::no,
                                tracktion::graph::AllocateAudioBuffer::no });
        }
    }
}

void PluginNode::prefetchBlock (juce::Range<int64_t>)
{
    plugin->prepareForNextBlock (getEditTimeRange().getStart());
}

void PluginNode::preProcess (choc::buffer::FrameCount, juce::Range<int64_t>)
{
    if (canUseSourceBuffers)
        setBufferViewToUse (input.get(), input->getProcessedOutput().audio);
}

void PluginNode::process (ProcessContext& pc)
{
    auto inputBuffers = input->getProcessedOutput();
    auto& inputAudioBlock = inputBuffers.audio;

    auto& outputBuffers = pc.buffers;
    auto outputAudioView = outputBuffers.audio;
    const auto blockNumSamples = inputAudioBlock.getNumFrames();
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
        tracktion::graph::copyIfNotAliased (outputAudioView.getFirstChannels (numInputChannelsToCopy),
                                            inputAudioBlock.getFirstChannels (numInputChannelsToCopy));

    // Init block
    auto subBlockSize = subBlockSizeToUse < 0 ? blockNumSamples
                                              : (choc::buffer::FrameCount) subBlockSizeToUse;

    choc::buffer::FrameCount numSamplesDone = 0;
    auto numSamplesLeft = blockNumSamples;

    bool shouldProcessPlugin = canProcessBypassed || plugin->isEnabled();
    bool isAllNotesOff = inputBuffers.midi.isAllNotesOff || playHeadState.didPlayheadJump();

    if (trackMuteState != nullptr)
    {
        if (! trackMuteState->shouldTrackContentsBeProcessed())
        {
            shouldProcessPlugin = shouldProcessPlugin && trackMuteState->shouldTrackBeAudible();
            isAllNotesOff = isAllNotesOff || trackMuteState->wasJustMuted();
        }
    }

    // Fast path for disabled plugins: skip sub-block loop and pass MIDI straight through
    if (! shouldProcessPlugin)
    {
        outputBuffers.midi.copyFrom (inputBuffers.midi);
        outputBuffers.midi.isAllNotesOff = isAllNotesOff;
    }
    else
    {
        const auto blockTimeRange = getEditTimeRange();
        auto inputMidiIter = inputBuffers.midi.begin();

        // Process in blocks
        for (int subBlockNum = 0;; ++subBlockNum)
        {
            auto numSamplesThisBlock = std::min (subBlockSize, numSamplesLeft);

            auto outputAudioBuffer = toAudioBuffer (outputAudioView.getFrameRange (frameRangeWithStartAndLength (numSamplesDone, numSamplesThisBlock)));

            const auto blockPropStart = (numSamplesDone / (double) blockNumSamples);
            const auto blockPropEnd = ((numSamplesDone + numSamplesThisBlock) / (double) blockNumSamples);
            const auto subBlockTimeRange = TimeRange (toPosition (blockTimeRange.getLength()) * blockPropStart,
                                                      toPosition (blockTimeRange.getLength()) * blockPropEnd);

            midiMessageArray.clear();
            midiMessageArray.isAllNotesOff = isAllNotesOff;

            for (auto end = inputBuffers.midi.end(); inputMidiIter != end; ++inputMidiIter)
            {
                const auto timestamp = inputMidiIter->getTimeStamp();

                // If the time range is empty, we need to pass through all the MIDI as it means the playhead is stopped
                if (! subBlockTimeRange.isEmpty()
                    && timestamp >= subBlockTimeRange.getEnd().inSeconds())
                   break;

                midiMessageArray.addMidiMessage (*inputMidiIter,
                                                 timestamp - subBlockTimeRange.getStart().inSeconds(),
                                                 inputMidiIter->mpeSourceID);
            }

            // Process the plugin
            plugin->applyToBufferWithAutomation (getPluginRenderContext ({ blockTimeRange.getStart() + toDuration (subBlockTimeRange.getStart()),
                                                                           blockTimeRange.getStart() + toDuration (subBlockTimeRange.getEnd()) },
                                                                         outputAudioBuffer));

            // Then copy the buffers to the outputs
            if (subBlockNum == 0)
                outputBuffers.midi.swapWith (midiMessageArray);
            else
                outputBuffers.midi.mergeFrom (midiMessageArray);

            numSamplesDone += numSamplesThisBlock;
            numSamplesLeft -= numSamplesThisBlock;

            if (numSamplesLeft == 0)
                break;

            isAllNotesOff = false;
        }
    }

    // If the plugin was bypassed, use the delayed audio
    if (latencyProcessor)
    {
        // A slightly better approach would be to crossfade between the processed and latency block to minimise any discrepancies
        if (plugin->isEnabled())
        {
            auto numSamples = (int) blockNumSamples;
            latencyProcessor->clearAudio (numSamples);
            latencyProcessor->clearMIDI (numSamples);
        }
        else
        {
            outputBuffers.midi.clear();

            // If no inputs have been added to the fifo, there won't be any samples available so skip
            if (numInputChannelsToCopy > 0)
                latencyProcessor->readAudioOverwriting (outputAudioView);

            latencyProcessor->readMIDI (outputBuffers.midi, (int) blockNumSamples);
        }
    }

   #if TRACKTION_SANITISE_PLUGIN_OUTPUT
    // Some plugins flake and add NaNs so zero these out to avoid killing all the audio downstream
    sanitise (outputAudioView);
   #endif
}

//==============================================================================
void PluginNode::initialisePlugin (double sampleRateToUse, int blockSizeToUse)
{
    plugin->baseClassInitialise ({ 0_tp, sampleRateToUse, blockSizeToUse });
    isInitialised = true;

    sampleRate = sampleRateToUse;
    latencyNumSamples = juce::roundToInt (plugin->getLatencySeconds() * sampleRate);
}

PluginRenderContext PluginNode::getPluginRenderContext (TimeRange editTime, juce::AudioBuffer<float>& destBuffer)
{
    return { &destBuffer,
             0, destBuffer.getNumSamples(),
             &midiMessageArray, 0.0,
             editTime + automationAdjustmentTime,
             playHeadState.playHead.isPlaying(), playHeadState.playHead.isUserDragging(),
             isRendering, canProcessBypassed };
}

void PluginNode::replaceLatencyProcessorIfPossible (NodeGraph* nodeGraphToReplace)
{
    if (nodeGraphToReplace == nullptr)
        return;

    const auto props = getNodeProperties();
    const auto nodeIDToLookFor = props.nodeID;

    if (nodeIDToLookFor == 0)
        return;

    if (auto oldNode = findNodeWithID<PluginNode> (*nodeGraphToReplace, nodeIDToLookFor))
    {
        if (! oldNode->latencyProcessor)
            return;

        if (! latencyProcessor)
        {
            if (oldNode->latencyProcessor->hasConfiguration (latencyNumSamples, sampleRate, props.numberOfChannels))
                latencyProcessor = oldNode->latencyProcessor;

            return;
        }

        if (latencyProcessor->hasSameConfigurationAs (*oldNode->latencyProcessor))
            latencyProcessor = oldNode->latencyProcessor;
    }
}

} // namespace tracktion::inline engine
