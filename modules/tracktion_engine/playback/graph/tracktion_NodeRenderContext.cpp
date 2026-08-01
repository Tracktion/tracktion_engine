/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine
{

namespace
{
    static Plugin::Array findAllPlugins (tracktion::graph::Node& node)
    {
        Plugin::Array plugins, insideRacks;

        for (auto n : getNodes (node, VertexOrdering::preordering))
            if (auto pluginNode = dynamic_cast<PluginNode*> (n))
                plugins.add (&pluginNode->getPlugin());

        for (auto plugin : plugins)
            if (auto rack = dynamic_cast<RackInstance*> (plugin))
                if (auto type = rack->type)
                    for (auto p : type->getPlugins())
                        insideRacks.addIfNotAlreadyThere (p);

        plugins.addArray (insideRacks);
        return plugins;
    }
}


//==============================================================================
NodeRenderContext::NodeRenderContext (Renderer::RenderTask& owner_, Renderer::Parameters& p,
                                      std::unique_ptr<Node> n,
                                      std::unique_ptr<tracktion::graph::PlayHead> playHead_,
                                      std::unique_ptr<tracktion::graph::PlayHeadState> playHeadState_,
                                      std::unique_ptr<ProcessState> processState_,
                                      juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate_)
    : owner (owner_),
      r (p), originalParams (p),
      playHead (std::move (playHead_)),
      playHeadState (std::move (playHeadState_)),
      processState (std::move (processState_)),
      status (juce::Result::ok()),
      ditherers (256, r.bitDepth),
      sourceToUpdate (sourceToUpdate_)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (r.engine != nullptr);
    jassert (r.edit != nullptr);
    jassert (r.time.getLength() > 0.0s);

    nodePlayer = std::make_unique<TracktionNodePlayer> (std::move (n), *processState, r.sampleRateForAudio, r.blockSizeForAudio,
                                                        getPoolCreatorFunction (static_cast<tracktion::graph::ThreadPoolStrategy> (EditPlaybackContext::getThreadPoolStrategy())));
    nodePlayer->setNumThreads ((size_t) p.engine->getEngineBehaviour().getNumberOfCPUsToUseForAudio() - 1);

    numLatencySamplesToDrop = nodePlayer->getNode()->getNodeProperties().latencyNumSamples;
    r.time = r.time.withEnd (r.time.getEnd() + TimeDuration::fromSamples (numLatencySamplesToDrop, r.sampleRateForAudio));

    if (r.edit->getTransport().isPlayContextActive())
    {
        jassertfalse;
        TRACKTION_LOG_ERROR("Rendering whilst attached to audio device");
    }

    if (r.shouldNormalise || r.trimSilenceAtEnds || r.shouldNormaliseByRMS || r.shouldNormaliseByLUFS || r.wrapRemainder)
    {
        needsToNormaliseAndTrim = true;

        r.audioFormat = r.engine->getAudioFileFormatManager().getFrozenFileFormat();

        intermediateFile = std::make_unique<juce::TemporaryFile> (r.destFile.withFileExtension (r.audioFormat->getFileExtensions()[0]));
        r.destFile = intermediateFile->getFile();

        r.shouldNormalise = false;
        r.trimSilenceAtEnds = false;
        r.shouldNormaliseByRMS = false;
        r.shouldNormaliseByLUFS = false;
    }

    ChannelConfiguration outputChannelConfig;

    {
        auto props = nodePlayer->getNode()->getNodeProperties();

        if (p.checkNodesForAudio && ! props.hasAudio)
        {
            status = juce::Result::fail (TRANS("Didn't find any audio to render"));
            return;
        }

        if (r.mustRenderInMono || (r.canRenderInMono && (props.numberOfChannels < 2)))
            outputChannelConfig = ChannelConfiguration::mono();
        else if (! r.channelConfig.isEmpty())
            outputChannelConfig = r.channelConfig;
        else
            outputChannelConfig = ChannelConfiguration::canonical (juce::jmax (2, (int) props.numberOfChannels));
    }

    numOutputChans = outputChannelConfig.getNumChannels();

    // Build a channel map from the graph's output order to the requested output order.
    // Each entry in outputChannelConfig has an indexInDevice which tells us which graph
    // channel should map to that output position.
    {
        auto graphChans = (int) nodePlayer->getNode()->getNodeProperties().numberOfChannels;

        for (int destCh = 0; destCh < numOutputChans; ++destCh)
        {
            auto srcCh = outputChannelConfig[(size_t) destCh].indexInDevice;

            if (srcCh >= 0 && srcCh < graphChans)
                outputChannelMap.entries.push_back ({ srcCh, destCh, 1.0f });
        }
    }

    AudioFileUtils::addBWAVStartToMetadata (r.metadata, toSamples (r.time.getStart(), r.sampleRateForAudio));

    // Try to create writer with channel layout metadata first
    writer = std::make_unique<AudioFileWriter> (AudioFile (*originalParams.engine, r.destFile),
                                                r.audioFormat, numOutputChans, r.sampleRateForAudio,
                                                r.bitDepth, r.metadata, r.quality,
                                                outputChannelConfig.toChannelSet());

    // Some formats don't support certain channel layouts — fall back to just numChannels
    if (! writer->isOpen())
        writer = std::make_unique<AudioFileWriter> (AudioFile (*originalParams.engine, r.destFile),
                                                    r.audioFormat, numOutputChans, r.sampleRateForAudio,
                                                    r.bitDepth, r.metadata, r.quality);

    if (r.destFile != juce::File() && ! writer->isOpen())
    {
        status = juce::Result::fail (TRANS("Couldn't write to target file"));
        return;
    }

    blockLength = TimeDuration::fromSamples (r.blockSizeForAudio, r.sampleRateForAudio);

    // number of blank blocks to play before starting, to give plugins time to warm up
    numPreRenderBlocks = (int) ((r.sampleRateForAudio / 2) / r.blockSizeForAudio + 1);

    // how long each block must take in real-time
    realTimePerBlock = (int) (blockLength.inSeconds() * 1000.0 + 0.99);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    sleepCounter = 10;

    currentTempoPosition = std::make_unique<tempo::Sequence::Position> (createPosition (r.edit->tempoSequence));

    peak = 0.0001f;
    rmsTotal = 0.0;
    rmsNumSamps = 0;
    streamTime = r.time.getStart();

    precount = numPreRenderBlocks;
    streamTime = streamTime - (blockLength * precount);

    plugins = findAllPlugins (*nodePlayer->getNode());

    // Set the realtime property before preparing to play
    Renderer::RenderTask::setAllPluginsRealtime (plugins, r.realTimeRender);
    nodePlayer->prepareToPlay (r.sampleRateForAudio, r.blockSizeForAudio);
    Renderer::RenderTask::flushAllPlugins (plugins, r.sampleRateForAudio, r.blockSizeForAudio);

    samplesTrimmed = 0;
    hasStartedSavingToFile = ! r.trimSilenceAtEnds;

    playHead->stop();
    playHead->setPosition (toSamples (r.time.getStart(), r.sampleRateForAudio));

    samplesToWrite = tracktion::toSamples ((r.time.getLength() + r.endAllowance), r.sampleRateForAudio);

    if (sourceToUpdate != nullptr)
        sourceToUpdate->reset (numOutputChans, r.sampleRateForAudio, samplesToWrite);
}

NodeRenderContext::~NodeRenderContext()
{
    CRASH_TRACER
    r.resultMagnitude = owner.params.resultMagnitude = peak;
    r.resultRMS = owner.params.resultRMS = rmsNumSamps > 0 ? (float) (rmsTotal / static_cast<double> (rmsNumSamps)) : 0.0f;
    r.resultAudioDuration = owner.params.resultAudioDuration = float (static_cast<double> (numSamplesWrittenToSource) / owner.params.sampleRateForAudio);

    playHead->stop();
    Renderer::RenderTask::setAllPluginsRealtime (plugins, true);

    if (writer != nullptr)
        writer->closeForWriting();

    try
    {
        callBlocking ([this] { nodePlayer.reset(); });

        // Normalising a cancelled render re-reads and re-encodes the whole partial file
        // only for the result to be thrown away, so skip it and let the file be deleted
        if (needsToNormaliseAndTrim && ! owner.shouldCancel())
            owner.performNormalisingAndTrimming (originalParams, r);
    }
    catch (const std::exception& err)
    {
        // Destructors are implicitly noexcept, so anything escaping here (a bad_alloc
        // from the normalising pass, say) would terminate rather than fail the render
        TRACKTION_LOG_ERROR (err.what());
    }
}

bool NodeRenderContext::renderNextBlock (std::atomic<float>& progressToUpdate)
{
    CRASH_TRACER
    jassert (! r.edit->getTransport().isPlayContextActive());

    if (--sleepCounter <= 0)
    {
        sleepCounter = sleepCounterMax;
        juce::Thread::sleep (1);
    }

    if (owner.shouldCancel())
    {
        writer->closeForWriting();
        r.destFile.deleteFile();

        playHead->stop();
        Renderer::RenderTask::setAllPluginsRealtime (plugins, true);

        // This reports the job as finished, so without an error the caller would be
        // told a render whose file has just been deleted had succeeded (as renderMidi)
        owner.errorMessage = TRANS("Render cancelled");

        return true;
    }

    auto blockEnd = streamTime + blockLength;

    if (precount > 0)
        blockEnd = juce::jmin (r.time.getStart(), blockEnd);

    if (precount > numPreRenderBlocks / 2)
        playHead->setPosition (toSamples (streamTime, r.sampleRateForAudio));
    else if (precount == numPreRenderBlocks / 2)
        playHead->playSyncedToRange ({ toSamples (streamTime, r.sampleRateForAudio), std::numeric_limits<int64_t>::max() });

    if (precount == 0)
    {
        streamTime = r.time.getStart();
        blockEnd = streamTime + blockLength;

        playHead->playSyncedToRange (toSamples (TimeRange (streamTime, Edit::getMaximumLength()), r.sampleRateForAudio));
        playHeadState->update (toSamples (TimeRange (streamTime, blockEnd), r.sampleRateForAudio));
    }

    if (r.realTimeRender)
    {
        auto timeNow = juce::Time::getMillisecondCounterHiRes();
        auto timeToWait = (int) (realTimePerBlock - (timeNow - lastTime));
        lastTime = timeNow;

        if (timeToWait > 0)
            juce::Thread::sleep (timeToWait);
    }

    currentTempoPosition->set (streamTime);

    resetFP();

    const TimeRange streamTimeRange (streamTime, blockEnd);
    const auto referenceSampleRange = juce::Range<int64_t>::withStartAndLength (toSamples (streamTimeRange.getStart(), originalParams.sampleRateForAudio), r.blockSizeForAudio);

    // Update modifier timers
    r.edit->updateModifierTimers (streamTime, r.blockSizeForAudio);

    // Wait for any nodes to render their sources or proxies
    auto leafNodesReady = [this, referenceSampleRange]
    {
        for (auto node : getNodes (*nodePlayer->getNode(), VertexOrdering::postordering))
        {
            // Call prepare for next block here to ensure isReadyToProcess internals are updated
            node->prepareForNextBlock (referenceSampleRange);

            if (node->getDirectInputNodes().empty() && ! node->isReadyToProcess())
                return false;
        }

        return true;
    }();

    while (! (leafNodesReady || owner.shouldCancel()))
        return false;

    const auto bufferSize = r.blockSizeForAudio + 256;
    const auto numFrames = (choc::buffer::FrameCount) referenceSampleRange.getLength();
    const bool needsChannelRemap = ! outputChannelMap.isEmpty() && ! outputChannelMap.isIdentity();

    // Allocate graph buffer (may differ from output if remapping is needed)
    // Must be large enough for both the graph's output and the remap source channels
    auto maxSourceChan = [&]
    {
        int maxSrc = 0;

        for (const auto& e : outputChannelMap.entries)
            maxSrc = juce::jmax (maxSrc, e.source + 1);

        return maxSrc;
    }();
    const int graphBufferChans = needsChannelRemap ? juce::jmax (maxSourceChan, numOutputChans) : numOutputChans;
    juce::AudioBuffer<float> renderingBuffer (graphBufferChans, bufferSize);
    renderingBuffer.clear();
    midiBuffer.clear();

    auto destView = choc::buffer::createChannelArrayView (renderingBuffer.getArrayOfWritePointers(),
                                                          (choc::buffer::ChannelCount) graphBufferChans,
                                                          numFrames);

    nodePlayer->process ({ numFrames, referenceSampleRange, { destView, midiBuffer} });

    // Remap channels from graph order to requested output order
    if (needsChannelRemap)
    {
        juce::AudioBuffer<float> remappedBuffer (numOutputChans, (int) numFrames);
        remappedBuffer.clear();

        for (const auto& entry : outputChannelMap.entries)
            if (entry.source < graphBufferChans && entry.dest < numOutputChans)
                remappedBuffer.addFrom (entry.dest, 0, renderingBuffer, entry.source, 0, (int) numFrames, entry.gain);

        // Copy remapped data back into renderingBuffer (resized to output channel count)
        renderingBuffer.setSize (numOutputChans, (int) numFrames, false, false, true);

        for (int ch = 0; ch < numOutputChans; ++ch)
            renderingBuffer.copyFrom (ch, 0, remappedBuffer, ch, 0, (int) numFrames);
    }

    if (precount <= 0)
    {
        jassert (playHeadState->isContiguousWithPreviousBlock());

        auto numSamplesDone = (uint32_t) juce::jmin (samplesToWrite, (int64_t) r.blockSizeForAudio);
        samplesToWrite -= numSamplesDone;

        auto blockSize = (uint32_t) numSamplesDone;
        uint32_t blockOffset = 0;

        if (numLatencySamplesToDrop > 0)
        {
            auto numToDrop = std::min ((uint32_t) numLatencySamplesToDrop, numSamplesDone);
            numLatencySamplesToDrop -= (int) numToDrop;
            numSamplesDone -= numToDrop;

            blockSize = numSamplesDone;
            blockOffset = destView.getNumFrames() - blockSize;
        }

        if (blockSize > 0)
        {
            jassert (blockSize <= destView.getNumFrames());

            if (writeAudioBlock (destView.getFrameRange ({ blockOffset, blockOffset + blockSize })) == WriteResult::failed)
                return true;
        }
    }
    else
    {
        // for the pre-count blocks, sleep to give things a chance to get going
        juce::Thread::sleep ((int) (blockLength.inSeconds() * 1000));
    }

    if (streamTime > r.time.getEnd() + r.endAllowance)
    {
        // Ending after end time and end allowance has elapsed
        return true;
    }
    else if (streamTime > r.time.getEnd()
             && renderingBuffer.getMagnitude (0, r.blockSizeForAudio) <= thresholdForStopping)
    {
        // Ending during end allowance period due to low magnitude
        return true;
    }

    auto prog = (float) ((streamTime - r.time.getStart()) / juce::jmax (1_td, r.time.getLength()));

    if (needsToNormaliseAndTrim)
        prog *= 0.9f;

    jassert (! std::isnan (prog));
    progressToUpdate = juce::jlimit (0.0f, 1.0f, prog);
    --precount;
    streamTime = blockEnd;

    return false;
}

//==============================================================================
NodeRenderContext::WriteResult NodeRenderContext::writeAudioBlock (choc::buffer::ChannelArrayView<float> block)
{
    CRASH_TRACER
    // Prepare buffer to use
    auto blockSizeSamples = (int) block.getNumFrames();

    juce::AudioBuffer<float> buffer (block.data.channels, numOutputChans, blockSizeSamples);

    // Apply dithering and mag/rms analysis
    if (r.ditheringEnabled && r.bitDepth < 32)
        ditherers.apply (buffer, blockSizeSamples);

    auto mag = buffer.getMagnitude (0, blockSizeSamples);
    peak = juce::jmax (peak, mag);

    if (! hasStartedSavingToFile)
        hasStartedSavingToFile = (mag > 0.0f);

    for (int i = buffer.getNumChannels(); --i >= 0;)
    {
        rmsTotal += buffer.getRMSLevel (i, 0, blockSizeSamples);
        ++rmsNumSamps;
    }

    if (! hasStartedSavingToFile)
        samplesTrimmed += blockSizeSamples;

    // Update thumbnail source
    if (sourceToUpdate != nullptr && blockSizeSamples > 0)
        sourceToUpdate->addBlock (numSamplesWrittenToSource, buffer, 0, blockSizeSamples);

    numSamplesWrittenToSource += blockSizeSamples;

    // And finally write to the file
    // NB buffer gets trashed by this call
    if (blockSizeSamples > 0 && hasStartedSavingToFile
         && writer->isOpen()
         && ! writer->appendBuffer (buffer, blockSizeSamples))
        return WriteResult::failed;

    return WriteResult::succeeded;
}

//==============================================================================
juce::String NodeRenderContext::renderMidi (Renderer::RenderTask& owner,
                                            Renderer::Parameters& r,
                                            std::unique_ptr<tracktion::graph::Node> n,
                                            std::unique_ptr<tracktion::graph::PlayHead> playHead,
                                            std::unique_ptr<tracktion::graph::PlayHeadState> playHeadState,
                                            std::unique_ptr<ProcessState> processState,
                                            std::atomic<float>& progress)
{
    const int samplesPerBlock = r.blockSizeForAudio;
    const double sampleRate = r.sampleRateForAudio;
    const auto blockLength = TimeDuration::fromSamples (samplesPerBlock, sampleRate);
    auto streamTime = r.time.getStart();

    std::unique_ptr<TracktionNodePlayer> nodePlayer;
    callBlocking ([&]
                  {
                      nodePlayer = std::make_unique<TracktionNodePlayer> (std::move (n), *processState,
                                                                          sampleRate, samplesPerBlock,
                                                                          getPoolCreatorFunction (static_cast<tracktion::graph::ThreadPoolStrategy> (EditPlaybackContext::getThreadPoolStrategy())));
                  });
    // Ensure the node player gets deleted on the message thread
    const juce::ErasedScopeGuard scope ([&nodePlayer] { callBlocking ([&] { nodePlayer.reset(); }); });

    nodePlayer->setNumThreads ((size_t) r.engine->getEngineBehaviour().getNumberOfCPUsToUseForAudio() - 1);

    //TODO: Should really purge any non-MIDI nodes here then return if no MIDI has been found

    playHead->stop();
    playHead->setPosition (toSamples (streamTime, sampleRate));
    playHead->playSyncedToRange (toSamples ({ streamTime, Edit::getMaximumLength() }, sampleRate));

    playHeadState->update (toSamples ({ streamTime, streamTime + blockLength }, sampleRate));

    // Wait for any nodes to render their sources or proxies
    auto leafNodesReady = [nodes = getNodes (*nodePlayer->getNode(), VertexOrdering::postordering)]
    {
        for (auto node : nodes)
            if (node->getDirectInputNodes().empty() && ! node->isReadyToProcess())
                return false;

        return true;
    };

    while (! leafNodesReady())
    {
        juce::Thread::sleep (100);

        if (owner.shouldCancel())
            return TRANS("Render cancelled");
    }

    // Then render the blocks
    auto currentTempoPosition = createPosition (r.edit->tempoSequence);

    juce::AudioBuffer<float> renderingBuffer (2, samplesPerBlock + 256);
    tracktion::engine::MidiMessageArray blockMidiBuffer;
    juce::MidiMessageSequence outputSequence;

    for (;;)
    {
        if (owner.shouldCancel())
            return TRANS("Render cancelled");

        if (streamTime > r.time.getEnd())
            break;

        auto blockEnd = streamTime + blockLength;
        const TimeRange streamTimeRange (streamTime, blockEnd);

        // Update modifier timers
        r.edit->updateModifierTimers (streamTime, samplesPerBlock);

        // Then once eveything is ready, render the block
        currentTempoPosition.set (streamTime);

        renderingBuffer.clear();
        blockMidiBuffer.clear();
        const auto referenceSampleRange = toSamples (streamTimeRange, sampleRate);
        auto destView = choc::buffer::createChannelArrayView (renderingBuffer.getArrayOfWritePointers(),
                                                              (choc::buffer::ChannelCount) renderingBuffer.getNumChannels(), (choc::buffer::FrameCount) referenceSampleRange.getLength());

        nodePlayer->process ({ (choc::buffer::FrameCount) referenceSampleRange.getLength(), referenceSampleRange, { destView, blockMidiBuffer} });

        // Set MIDI messages to beats and update final sequence
        for (auto& m : blockMidiBuffer)
        {
            tempo::Sequence::Position eventPos (currentTempoPosition);
            eventPos.set (TimePosition::fromSeconds (m.getTimeStamp()) + (streamTime - r.time.getStart()));

            outputSequence.addEvent (juce::MidiMessage (m, Edit::ticksPerQuarterNote * eventPos.getPPQTime()));
        }

        streamTime = blockEnd;

        progress = juce::jlimit (0.0f, 1.0f, (float) ((streamTime - r.time.getStart()) / r.time.getLength()));
    }

    playHead->stop();

    if (outputSequence.getNumEvents() == 0)
        return TRANS("No MIDI found to render");

    if (! Renderer::RenderTask::addMidiMetaDataAndWriteToFile (r.destFile, std::move (outputSequence), r.edit->tempoSequence))
        return TRANS("Unable to write to destination file");

    return {};
}


} // namespace tracktion::inline engine
