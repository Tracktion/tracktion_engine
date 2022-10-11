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
//==============================================================================
struct WaveNode::PerChannelState
{
    PerChannelState()    { resampler.reset(); }

    juce::LagrangeInterpolator resampler;
    float lastSample = 0;
};


//==============================================================================
WaveNode::WaveNode (const AudioFile& af,
                    EditTimeRange editTime,
                    double off,
                    EditTimeRange loop,
                    LiveClipLevel level,
                    double speed,
                    const juce::AudioChannelSet& channelSetToUse,
                    const juce::AudioChannelSet& destChannelsToFill,
                    ProcessState& ps,
                    EditItemID itemIDToUse,
                    bool isRendering)
   : TracktionEngineNode (ps),
     editPosition (editTime),
     loopSection (loop.getStart() * speed, loop.getEnd() * speed),
     offset (off),
     originalSpeedRatio (speed),
     editItemID (itemIDToUse),
     isOfflineRender (isRendering),
     audioFile (af),
     clipLevel (level),
     channelsToUse (channelSetToUse),
     destChannels (destChannelsToFill)
{
}

tracktion_graph::NodeProperties WaveNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = true;
    props.hasMidi = false;
    props.numberOfChannels = destChannels.size();
    props.nodeID = (size_t) editItemID.getRawID();

    return props;
}

void WaveNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    reader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);
    outputSampleRate = info.sampleRate;
    editPositionInSamples = tracktion_graph::timeToSample ({ editPosition.start, editPosition.end }, outputSampleRate);
    updateFileSampleRate();

    const int numChannelsToUse = std::max (channelsToUse.size(), reader != nullptr ? reader->getNumChannels() : 0);
    replaceChannelStateIfPossible (info.rootNodeToReplace, numChannelsToUse);

    if (! channelState)
    {
        channelState = std::make_shared<juce::OwnedArray<PerChannelState>>();

        if (reader != nullptr)
            for (int i = numChannelsToUse; --i >= 0;)
                channelState->add (new PerChannelState());
    }
}

bool WaveNode::isReadyToProcess()
{
    // Only check this whilst rendering or it will block whilst the proxies are being created
    if (! isOfflineRender)
        return true;
    
    // If the hash is 0 it means an empty file path which means a missing file so
    // this will never return a valid reader and we should just bail
    if (audioFile.isNull())
        return true;

    if (reader == nullptr)
    {
        reader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);

        if (reader == nullptr)
            return false;
    }

    if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
        return false;

    return true;
}

void WaveNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    assert (outputSampleRate == getSampleRate());

    //TODO: Might get a performance boost by pre-setting the file position in prepareForNextBlock
    processSection (pc, getTimelineSampleRange());
}

//==============================================================================
int64_t WaveNode::editPositionToFileSample (int64_t timelinePosition) const noexcept
{
    // Convert timelinePosition in samples to edit time
    return editTimeToFileSample (tracktion_graph::sampleToTime (timelinePosition, outputSampleRate));
}

int64_t WaveNode::editTimeToFileSample (double editTime) const noexcept
{
    return (int64_t) ((editTime - (editPosition.getStart() - offset))
                       * originalSpeedRatio * audioFileSampleRate + 0.5);
}

bool WaveNode::updateFileSampleRate()
{
    using namespace tracktion_graph;
    
    if (reader == nullptr)
        return false;

    audioFileSampleRate = reader->getSampleRate();

    if (audioFileSampleRate <= 0)
        return false;
    
    if (! loopSection.isEmpty())
        reader->setLoopRange ({ timeToSample (loopSection.getStart(), audioFileSampleRate),
                                timeToSample (loopSection.getEnd(), audioFileSampleRate) });

    return true;
}

void WaveNode::replaceChannelStateIfPossible (Node* rootNodeToReplace, int numChannelsToUse)
{
    if (rootNodeToReplace == nullptr)
        return;

    auto props = getNodeProperties();
    auto nodeIDToLookFor = props.nodeID;

    if (nodeIDToLookFor == 0)
        return;

    auto visitor = [this, nodeIDToLookFor, numChannelsToUse] (Node& node)
    {
        if (auto other = dynamic_cast<WaveNode*> (&node))
        {
            if (other->getNodeProperties().nodeID == nodeIDToLookFor)
            {
                if (! other->channelState)
                    return;

                if (other->channelState->size() == numChannelsToUse)
                    channelState = other->channelState;
            }
        }
    };
    visitNodes (*rootNodeToReplace, visitor, true);
}

void WaveNode::processSection (ProcessContext& pc, juce::Range<int64_t> timelineRange)
{
    const auto sectionEditTime = tracktion_graph::sampleToTime (timelineRange, outputSampleRate);
    
    if (reader == nullptr
         || sectionEditTime.getEnd() <= editPosition.getStart()
         || sectionEditTime.getStart() >= editPosition.getEnd())
        return;

    SCOPED_REALTIME_CHECK

    if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
        return;

    const auto fileStart       = editTimeToFileSample (sectionEditTime.getStart());
    const auto fileEnd         = editTimeToFileSample (sectionEditTime.getEnd());
    const auto numFileSamples  = (int) (fileEnd - fileStart);

    reader->setReadPosition (fileStart);

    auto destBuffer = pc.buffers.audio;
    auto numFrames = destBuffer.getNumFrames();
    const auto destBufferChannels = juce::AudioChannelSet::canonicalChannelSet ((int) destBuffer.getNumChannels());
    auto numChannels = (choc::buffer::ChannelCount) destBufferChannels.size();
    assert (pc.buffers.audio.getNumChannels() == numChannels);

    AudioScratchBuffer fileData ((int) numChannels, numFileSamples + 2);

    uint32_t lastSampleFadeLength = 0;

    {
        SCOPED_REALTIME_CHECK

        if (reader->readSamples (numFileSamples + 2, fileData.buffer, destBufferChannels, 0,
                                 channelsToUse,
                                 isOfflineRender ? 5000 : 3))
        {
            if (! getPlayHeadState().isContiguousWithPreviousBlock() && ! getPlayHeadState().isFirstBlockOfLoop())
                lastSampleFadeLength = std::min (numFrames, getPlayHead().isUserDragging() ? 40u : 10u);
        }
        else
        {
            lastSampleFadeLength = std::min (numFrames, 40u);
            fileData.buffer.clear();
        }
    }

    float gains[2];

    // For stereo, use the pan, otherwise ignore it
    if (numChannels == 2)
        clipLevel.getLeftAndRightGains (gains[0], gains[1]);
    else
        gains[0] = gains[1] = clipLevel.getGainIncludingMute();

    if (getPlayHead().isUserDragging())
    {
        gains[0] *= 0.4f;
        gains[1] *= 0.4f;
    }

    auto ratio = numFileSamples / (double) numFrames;

    if (ratio <= 0.0)
        return;

    jassert (numChannels <= (choc::buffer::ChannelCount) channelState->size()); // this should always have been made big enough

    for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
    {
        if (channel < (choc::buffer::ChannelCount) channelState->size())
        {
            const auto src = fileData.buffer.getReadPointer ((int) channel);
            const auto dest = destBuffer.getIterator (channel).sample;

            auto& state = *channelState->getUnchecked ((int) channel);
            state.resampler.processAdding (ratio, src, dest, (int) numFrames, gains[channel & 1]);

            if (lastSampleFadeLength > 0)
            {
                for (uint32_t i = 0; i < lastSampleFadeLength; ++i)
                {
                    auto alpha = i / (float) lastSampleFadeLength;
                    dest[i] = alpha * dest[i] + state.lastSample * (1.0f - alpha);
                }
            }

            state.lastSample = dest[numFrames - 1];
        }
        else
        {
            destBuffer.getChannel (channel).clear();
        }
    }
    
    // Silence any samples before or after our edit time range
    // N.B. this shouldn't happen when using a clip combiner as the times should be clipped correctly
    {
        auto numSamplesToClearAtStart = editPositionInSamples.getStart() - timelineRange.getStart();
        auto numSamplesToClearAtEnd = timelineRange.getEnd() - editPositionInSamples.getEnd();

        if (numSamplesToClearAtStart > 0)
            destBuffer.getStart ((choc::buffer::FrameCount) numSamplesToClearAtStart).clear();

        if (numSamplesToClearAtEnd > 0)
            destBuffer.getEnd ((choc::buffer::FrameCount) numSamplesToClearAtEnd).clear();
    }
}

}
