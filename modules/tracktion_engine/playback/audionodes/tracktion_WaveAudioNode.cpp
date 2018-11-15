/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


WaveAudioNode::WaveAudioNode (const AudioFile& af,
                              EditTimeRange editTime,
                              double off,
                              EditTimeRange loop,
                              LiveClipLevel level,
                              double speed,
                              const juce::AudioChannelSet& channelSetToUse)
   : editPosition (editTime),
     loopSection (loop.getStart() * speed, loop.getEnd() * speed),
     offset (off),
     originalSpeedRatio (speed),
     audioFile (af),
     clipLevel (level),
     channelsToUse (channelSetToUse)
{
}

WaveAudioNode::~WaveAudioNode()
{
}

//==============================================================================
void WaveAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    info.hasAudio = true;
    info.hasMidi = false;
    info.numberOfChannels = jlimit (1, std::max (channelsToUse.size(), 1), audioFile.getNumChannels());
}

bool WaveAudioNode::purgeSubNodes (bool keepAudio, bool)
{
    return keepAudio;
}

void WaveAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);
}

int64 WaveAudioNode::editTimeToFileSample (double editTime) const noexcept
{
    return (int64) ((editTime - (editPosition.getStart() - offset))
                      * originalSpeedRatio * audioFileSampleRate + 0.5);
}

struct WaveAudioNode::PerChannelState
{
    PerChannelState()    { resampler.reset(); }

    juce::LagrangeInterpolator resampler;
    float lastSample = 0;
};

void WaveAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    reader = Engine::getInstance().getAudioFileManager().cache.createReader (audioFile);
    outputSampleRate = info.sampleRate;
    updateFileSampleRate();

    channelState.clear();

    if (reader != nullptr)
        for (int i = std::max (channelsToUse.size(), reader->getNumChannels()); --i >= 0;)
            channelState.add (new PerChannelState());
}

bool WaveAudioNode::isReadyToRender()
{
    // if the hash is 0 it means an empty file path which means a missing file so
    // this will never return a valid reader and we should just bail
    if (audioFile.isNull())
        return true;

    if (reader == nullptr)
    {
        reader = Engine::getInstance().getAudioFileManager().cache.createReader (audioFile);

        if (reader == nullptr)
            return false;
    }

    if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
        return false;

    return true;
}

bool WaveAudioNode::updateFileSampleRate()
{
    if (reader != nullptr)
    {
        audioFileSampleRate = reader->getSampleRate();

        if (audioFileSampleRate > 0)
        {
            if (! loopSection.isEmpty())
                reader->setLoopRange (Range<int64> ((int64) (loopSection.getStart() * audioFileSampleRate),
                                                    (int64) (loopSection.getEnd()   * audioFileSampleRate)));

            return true;
        }
    }

    return false;
}

void WaveAudioNode::releaseAudioNodeResources()
{
    reader = nullptr;
}

void WaveAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void WaveAudioNode::renderAdding (const AudioRenderContext& rc)
{
    invokeSplitRender (rc, *this);
}

void WaveAudioNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
{
    // keep a local copy, because releaseAudioNodeResources may remove the reader halfway through..
    const auto localReader = reader;

    rc.sanityCheck();

    if (rc.destBuffer == nullptr
         || rc.bufferNumSamples == 0
         || localReader == nullptr
         || editTime.getStart() >= editPosition.getEnd())
        return;

    SCOPED_REALTIME_CHECK

    if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
        return;

    const auto fileStart       = editTimeToFileSample (editTime.getStart());
    const auto fileEnd         = editTimeToFileSample (editTime.getEnd());
    const auto numFileSamples  = (int) (fileEnd - fileStart);

    localReader->setReadPosition (fileStart);

    AudioScratchBuffer fileData (rc.destBufferChannels.size(), numFileSamples + 2);

    int lastSampleFadeLength = 0;

    {
        SCOPED_REALTIME_CHECK

        if (localReader->readSamples (numFileSamples + 2, fileData.buffer, rc.destBufferChannels, 0,
                                      channelsToUse,
                                      rc.isRendering ? 5000 : 3))
        {
            if (rc.isFirstBlockOfLoop() || ! rc.isContiguousWithPreviousBlock())
                lastSampleFadeLength = std::min (rc.bufferNumSamples, rc.playhead.isUserDragging() ? 40 : 10);
        }
        else
        {
            lastSampleFadeLength = std::min (rc.bufferNumSamples, 40);
            fileData.buffer.clear();
        }
    }

    float gains[2];

    // For stereo, use the pan, otherwise ignore it
    if (rc.destBuffer->getNumChannels() == 2)
        clipLevel.getLeftAndRightGains (gains[0], gains[1]);
    else
        gains[0] = gains[1] = clipLevel.getGainIncludingMute();

    if (rc.playhead.isUserDragging())
    {
        gains[0] *= 0.4f;
        gains[1] *= 0.4f;
    }

    auto ratio = numFileSamples / (double) rc.bufferNumSamples;

    if (ratio > 0.0)
    {
        auto numDestChannels = std::min (rc.destBuffer->getNumChannels(), fileData.buffer.getNumChannels());
        jassert (numDestChannels <= channelState.size()); // this should always have been made big enough

        for (int channel = 0; channel < numDestChannels; ++channel)
        {
            const auto src = fileData.buffer.getReadPointer (channel);
            const auto dest = rc.destBuffer->getWritePointer (channel, rc.bufferStartSample);

            auto& state = *channelState.getUnchecked (channel);
            state.resampler.processAdding (ratio, src, dest, rc.bufferNumSamples, gains[channel & 1]);

            if (lastSampleFadeLength > 0)
            {
                for (int i = 0; i < lastSampleFadeLength; ++i)
                {
                    auto alpha = i / (float) lastSampleFadeLength;
                    dest[i] = alpha * dest[i] + state.lastSample * (1.0f - alpha);
                }
            }

            state.lastSample = dest[rc.bufferNumSamples - 1];
        }
    }
}

void WaveAudioNode::prepareForNextBlock (const AudioRenderContext& rc)
{
    SCOPED_REALTIME_CHECK

    // keep a local copy, because releaseAudioNodeResources may remove the reader halfway through..
    if (auto localReader = reader)
        localReader->setReadPosition (editTimeToFileSample (rc.getEditTime().editRange1.getStart()));
}
