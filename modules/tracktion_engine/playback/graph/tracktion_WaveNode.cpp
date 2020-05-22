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

enum class ContinuityFlags
{
    contiguous           = 1,
    playheadJumped       = 2,
    lastBlockBeforeLoop  = 4,
    firstBlockOfLoop     = 8
};

namespace Continuity
{
    static inline bool isContiguousWithPreviousBlock (int continuityFlags) noexcept    { return (continuityFlags & (int) ContinuityFlags::contiguous) != 0; }
    static inline bool isFirstBlockOfLoop (int continuityFlags) noexcept               { return (continuityFlags & (int) ContinuityFlags::firstBlockOfLoop) != 0; }
    static inline bool isLastBlockOfLoop (int continuityFlags) noexcept                { return (continuityFlags & (int) ContinuityFlags::lastBlockBeforeLoop) != 0; }
    static inline bool didPlayheadJump (int continuityFlags) noexcept                  { return (continuityFlags & (int) ContinuityFlags::playheadJumped) != 0; }
}


//==============================================================================
//==============================================================================
template <typename CallbackType>
static void invokeSplitProcessor (const tracktion_graph::Node::ProcessContext& pc, tracktion_graph::PlayHead& playHead, CallbackType& target)
{
    const auto splitTimelineRange = tracktion_graph::referenceSampleRangeToSplitTimelineRange (playHead, pc.streamSampleRange);
    int continuity = (int) ContinuityFlags::contiguous;
    
    if (splitTimelineRange.isSplit)
    {
        const auto firstNumSamples = splitTimelineRange.timelineRange1.getLength();
        const auto firstRange = pc.streamSampleRange.withLength (firstNumSamples);
        
        {
            auto inputAudio = pc.buffers.audio.getSubBlock (0, (size_t) firstNumSamples);
            auto& inputMidi = pc.buffers.midi;
            
            tracktion_graph::Node::ProcessContext pc1 { firstRange, { inputAudio , inputMidi } };
            continuity |= (int) ContinuityFlags::lastBlockBeforeLoop;
            target.processSection (pc1, splitTimelineRange.timelineRange1, continuity);
        }
        
        {
            const auto secondNumSamples = splitTimelineRange.timelineRange2.getLength();
            const auto secondRange = juce::Range<int64_t>::withStartAndLength (firstRange.getEnd(), secondNumSamples);
            
            auto inputAudio = pc.buffers.audio.getSubBlock ((size_t) firstNumSamples, (size_t) secondNumSamples);
            auto& inputMidi = pc.buffers.midi;
            
            //TODO: Use a scratch MidiMessageArray and then merge it back with the offset time
            tracktion_graph::Node::ProcessContext pc2 { secondRange, { inputAudio , inputMidi } };
            continuity = (int) ContinuityFlags::firstBlockOfLoop;
            target.processSection (pc2, splitTimelineRange.timelineRange2, continuity);
        }
    }
    else if (playHead.isLooping() && ! playHead.isRollingIntoLoop())
    {
        // In the case where looping happens to line up exactly with the audio
        // blocks being rendered, set the proper continuity flags
        auto loop = playHead.getLoopRange();

        auto s = splitTimelineRange.timelineRange1.getStart();
        auto e = splitTimelineRange.timelineRange1.getEnd();

        if (e == loop.getEnd())
            continuity |= (int) AudioRenderContext::lastBlockBeforeLoop;
        if (s == loop.getStart())
            continuity = (int) AudioRenderContext::firstBlockOfLoop;

        target.processSection (pc, splitTimelineRange.timelineRange1, continuity);
    }
    else
    {
        target.processSection (pc, splitTimelineRange.timelineRange1, continuity);
    }
}


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
                    tracktion_graph::PlayHead& ph,
                    bool isRendering)
   : playHead (ph),
     editPosition (editTime),
     loopSection (loop.getStart() * speed, loop.getEnd() * speed),
     offset (off),
     originalSpeedRatio (speed),
     isOfflineRender (isRendering),
     audioFile (af),
     clipLevel (level),
     channelsToUse (channelSetToUse)
{
}

tracktion_graph::NodeProperties WaveNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = true;
    props.hasMidi = false;
    props.numberOfChannels = juce::jlimit (1, std::max (channelsToUse.size(), 1), audioFile.getNumChannels());
    
    return props;
}

void WaveNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    reader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);
    outputSampleRate = info.sampleRate;
    editPositionInSamples = tracktion_graph::timeToSample ({ editPosition.start, editPosition.end }, outputSampleRate);
    updateFileSampleRate();

    channelState.clear();

    if (reader != nullptr)
        for (int i = std::max (channelsToUse.size(), reader->getNumChannels()); --i >= 0;)
            channelState.add (new PerChannelState());
}

bool WaveNode::isReadyToProcess()
{
    //TODO: This should only really be called whilst offline rendering as otherwise it will block whilst the proxies are being created
    
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

void WaveNode::process (const ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    //TODO: Might get a performance boost by pre-setting the file position in prepareForNextBlock
    invokeSplitProcessor (pc, playHead, *this);
}

//==============================================================================
int64_t WaveNode::editPositionToFileSample (int64_t timelinePosition) const noexcept
{
    // Convert timelinePosition in samples to edit time
    return editTimeToFileSample (timelinePosition / outputSampleRate);
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

void WaveNode::processSection (const ProcessContext& pc, juce::Range<int64_t> timelineRange, int continuityFlags)
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
    const int numSamples = (int) destBuffer.getNumSamples();
    const auto destBufferChannels = juce::AudioChannelSet::canonicalChannelSet ((int) destBuffer.getNumChannels());
    const int numChannels = destBufferChannels.size();
    assert ((int) pc.buffers.audio.getNumChannels() == numChannels);

    AudioScratchBuffer fileData (numChannels, numFileSamples + 2);

    int lastSampleFadeLength = 0;

    {
        SCOPED_REALTIME_CHECK

        if (reader->readSamples (numFileSamples + 2, fileData.buffer, destBufferChannels, 0,
                                 channelsToUse,
                                 isOfflineRender ? 5000 : 3))
        {
            if (! Continuity::isContiguousWithPreviousBlock (continuityFlags) && ! Continuity::isFirstBlockOfLoop (continuityFlags))
                lastSampleFadeLength = std::min (numSamples, playHead.isUserDragging() ? 40 : 10);
        }
        else
        {
            lastSampleFadeLength = std::min (numSamples, 40);
            fileData.buffer.clear();
        }
    }

    float gains[2];

    // For stereo, use the pan, otherwise ignore it
    if (numChannels == 2)
        clipLevel.getLeftAndRightGains (gains[0], gains[1]);
    else
        gains[0] = gains[1] = clipLevel.getGainIncludingMute();

    if (playHead.isUserDragging())
    {
        gains[0] *= 0.4f;
        gains[1] *= 0.4f;
    }

    auto ratio = numFileSamples / (double) numSamples;

    if (ratio <= 0.0)
        return;

    jassert (numChannels <= channelState.size()); // this should always have been made big enough

    for (int channel = 0; channel < numChannels; ++channel)
    {
        if (channel < channelState.size())
        {
            const auto src = fileData.buffer.getReadPointer (channel);
            const auto dest = destBuffer.getChannelPointer ((size_t) channel);

            auto& state = *channelState.getUnchecked (channel);
            state.resampler.processAdding (ratio, src, dest, numSamples, gains[channel & 1]);

            if (lastSampleFadeLength > 0)
            {
                for (int i = 0; i < lastSampleFadeLength; ++i)
                {
                    auto alpha = i / (float) lastSampleFadeLength;
                    dest[i] = alpha * dest[i] + state.lastSample * (1.0f - alpha);
                }
            }

            state.lastSample = dest[numSamples - 1];
        }
        else
        {
            destBuffer.getSubsetChannelBlock ((size_t) channel, 1).clear();
        }
    }
    
    // Silence any samples before or after our edit time range
    // N.B. this shouldn't happen when using a clip combiner as the times should be clipped correctly
    {
        const int64_t numSamplesToClearAtStart = editPositionInSamples.getStart() - timelineRange.getStart();
        const int64_t numSamplesToClearAtEnd = timelineRange.getEnd() - editPositionInSamples.getEnd();

        if (numSamplesToClearAtStart > 0)
            destBuffer.getSubBlock (0, (size_t) numSamplesToClearAtStart).clear();        

        if (numSamplesToClearAtEnd > 0)
            destBuffer.getSubBlock ((size_t) (numSamples - numSamplesToClearAtEnd), (size_t) numSamplesToClearAtEnd).clear();
    }
}

}
