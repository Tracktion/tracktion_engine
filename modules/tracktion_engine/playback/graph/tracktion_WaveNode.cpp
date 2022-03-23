/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class AudioFileCacheReader : public AudioReader
{
public:
    AudioFileCacheReader (AudioFileCache::Reader::Ptr ptr, TimeDuration timeout,
                          const juce::AudioChannelSet& destBufferChannels,
                          const juce::AudioChannelSet& sourceBufferChannels)
        : reader (ptr), timeoutMs ((int) std::lround (timeout.inSeconds() * 1000.0)),
          destChannelSet (destBufferChannels), sourceChannelSet (sourceBufferChannels)
    {
    }

    choc::buffer::ChannelCount getNumChannels() override
    {
        return numChannels;
    }

    SampleCount getPosition() override
    {
        return reader->getReadPosition();
    }

    void setPosition (SampleCount t) override
    {
        reader->setReadPosition (t);
    }

    void setPosition (TimePosition t) override
    {
        reader->setReadPosition (toSamples (t, getSampleRate()));
    }

    double getSampleRate() override
    {
        return reader->getSampleRate();
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        auto buffer = toAudioBuffer (destBuffer);
        return reader->readSamples ((int) destBuffer.getNumFrames(),
                                    buffer,
                                    destChannelSet,
                                    0,
                                    sourceChannelSet,
                                    timeoutMs);
    }

    AudioFileCache::Reader::Ptr reader;
    int timeoutMs;
    const juce::AudioChannelSet destChannelSet;
    const juce::AudioChannelSet sourceChannelSet;
    const choc::buffer::ChannelCount numChannels { static_cast<choc::buffer::ChannelCount> (destChannelSet.size()) };
};

//==============================================================================
class SingleInputAudioReader : public AudioReader
{
public:
    SingleInputAudioReader (std::unique_ptr<AudioReader> input)
        : source (std::move (input))
    {
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        return source->readSamples (destBuffer);
    }

    std::unique_ptr<AudioReader> source;
};

//==============================================================================
class LoopReader : public SingleInputAudioReader
{
public:
    LoopReader (std::unique_ptr<AudioReader> input, SampleRange loopRangeToUse)
        : SingleInputAudioReader (std::move (input)), loopRange (loopRangeToUse)
    {
    }

    LoopReader (std::unique_ptr<AudioReader> input, TimeRange loopRangeToUse)
        : SingleInputAudioReader (std::move (input)), loopRange (toSamples (loopRangeToUse, source->getSampleRate()))
    {
    }

    void setPosition (SampleCount t) override
    {
        const auto loopStart = loopRange.getStart();
        const auto loopLength = loopRange.getLength();

        if (loopLength > 0)
        {
            if (t >= 0)
                t = loopStart + (t % loopLength);
            else
                t = loopStart + juce::negativeAwareModulo (t, loopLength);
        }

        source->setPosition (t);
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        using choc::buffer::FrameCount;

        const auto loopStart = loopRange.getStart();
        const auto loopLength = loopRange.getLength();

        if (loopLength == 0)
            return source->readSamples (destBuffer);

        const auto numFrames = static_cast<SampleCount> (destBuffer.getNumFrames());

        auto readPos = source->getPosition();
        int numSamplesToDo = (int) numFrames;
        SampleCount startOffsetInDestBuffer = 0;
        bool allOk = true;

        while (numSamplesToDo > 0)
        {
            jassert (juce::isPositiveAndBelow (readPos - loopStart, loopLength));

            const auto numToRead = std::min ((SampleCount) numSamplesToDo, loopStart + loopLength - readPos);

            source->setPosition (readPos);
            auto destSubsection = destBuffer.getFrameRange ({ (FrameCount) startOffsetInDestBuffer, (FrameCount) (startOffsetInDestBuffer + numToRead) });
            allOk = source->readSamples (destSubsection) && allOk;

            readPos += numToRead;

            if (readPos >= loopStart + loopLength)
                readPos -= loopLength;

            startOffsetInDestBuffer += numToRead;
            numSamplesToDo -= numToRead;
        }

        return allOk;
    }

    const SampleRange loopRange;
};


//==============================================================================
class OffsetReader : public SingleInputAudioReader
{
public:
    OffsetReader (std::unique_ptr<AudioReader> input, SampleCount startSampleInFile)
        : SingleInputAudioReader (std::move (input)), startSample (startSampleInFile)
    {
    }

    OffsetReader (std::unique_ptr<AudioReader> input, TimePosition startTimeInSource)
        : SingleInputAudioReader (std::move (input)), startSample (toSamples (startTimeInSource, source->getSampleRate()))
    {
    }

    void setPosition (SampleCount t) override
    {
        source->setPosition (t + startSample);
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    SampleCount startSample;
};


//==============================================================================
class ResamplerReader : public SingleInputAudioReader
{
public:
    ResamplerReader (std::unique_ptr<AudioReader> input, double sampleRateToConvertTo)
        : SingleInputAudioReader (std::move (input)), destSampleRate (sampleRateToConvertTo)
    {
        for (int i = (int) source->getNumChannels(); --i >= 0;)
        {
            resamplers.emplace_back (juce::LagrangeInterpolator());
            resamplers.back().reset();
        }
    }

    void setPosition (SampleCount t) override
    {
        setPosition (TimePosition::fromSamples (t, destSampleRate));
    }

    void setPosition (TimePosition t) override
    {
        source->setPosition (t);
    }

    /** Sets a ratio to increase or decrease playback speed. */
    void setSpeedRatio (double newSpeedRatio)
    {
        assert (newSpeedRatio > 0);
        speedRatio = newSpeedRatio;
    }

    /** Sets a l/r gain to apply to channels. */
    void setGains (float leftGain, float rightGain)
    {
        gains[0] = leftGain;
        gains[1] = rightGain;
    }

    double getSampleRate() override
    {
        return destSampleRate;
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        const auto numChannels = destBuffer.getNumChannels();
        assert (numChannels <= (choc::buffer::ChannelCount) resamplers.size());
        assert (destBuffer.getNumChannels() == numChannels);

        const auto ratio = sampleRatio * speedRatio;
        const auto numFrames = destBuffer.getNumFrames();
        const int numSourceFramesToRead = static_cast<int> ((numFrames * ratio) + 0.5);
        AudioScratchBuffer fileData ((int) numChannels, numSourceFramesToRead);
        auto fileDataView = toBufferView (fileData.buffer);
        const bool ok = source->readSamples (fileDataView);

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            if (channel < (choc::buffer::ChannelCount) resamplers.size())
            {
                const auto src = fileData.buffer.getReadPointer ((int) channel);
                const auto dest = destBuffer.getIterator (channel).sample;

                auto& resampler = resamplers[(size_t) channel];
                resampler.processAdding (ratio, src, dest, (int) numFrames, gains[channel & 1]);
            }
            else
            {
                destBuffer.getChannel (channel).clear();
            }
        }

        return ok;
    }

    const double destSampleRate;
    const double sourceSampleRate { source->getSampleRate() };
    const double sampleRatio { sourceSampleRate / destSampleRate  };
    double speedRatio = 1.0;
    std::vector<juce::LagrangeInterpolator> resamplers;
    float gains[2] = { 1.0f, 1.0f };
};


//==============================================================================
class TimeStretchReader : public SingleInputAudioReader
{
public:
    TimeStretchReader (std::unique_ptr<AudioReader> input)
        : SingleInputAudioReader (std::move (input)), numChannels ((int) source->getNumChannels())
    {
        timeStretcher.initialise (source->getSampleRate(), chunkSize, numChannels,
                                  TimeStretcher::defaultMode, {}, true);
        inputFifo.setSize (numChannels, timeStretcher.getMaxFramesNeeded());
    }

    SampleCount getPosition() override
    {
        return getReadPosition();
    }

    void setPosition (SampleCount t) override
    {
        const auto readPos = getReadPosition();

        if (t == readPos)
            return;

        readPosition = static_cast<double> (t);
        source->setPosition (t);
        timeStretcher.reset();
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    void setSpeedAndPitch (double speedRatio, float semitones)
    {
        playbackSpeedRatio = speedRatio;
        [[ maybe_unused ]] const bool ok = timeStretcher.setSpeedAndPitch ((float) speedRatio, semitones);
        assert (ok);
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        assert (numChannels == (int) destBuffer.getNumChannels());
        const auto numFramesToDo = destBuffer.getNumFrames();

        for (;;)
        {
            // If there are enough output samples in the fifo, read them out
            if (outputFifo.getNumReady() >= int (numFramesToDo))
            {
                auto destAudioBuffer = toAudioBuffer (destBuffer);
                outputFifo.read (destAudioBuffer, 0);
                break;
            }

            const auto numThisTime = timeStretcher.getFramesNeeded();

            if (numThisTime > 0)
            {
                // Read samples from source and push to fifo
                AudioScratchBuffer scratchBuffer (numChannels, numThisTime);
                scratchBuffer.buffer.clear();
                auto scratchView = toBufferView (scratchBuffer.buffer);
                source->readSamples (scratchView);
                inputFifo.write (scratchBuffer.buffer);
            }

            // Push into time-stretcher and read output samples to out fifo
            assert (inputFifo.getNumReady() >= numThisTime);
            assert (outputFifo.getFreeSpace() >= numThisTime);
            assert (outputFifo.getFreeSpace() >= chunkSize);
            timeStretcher.processData (inputFifo, numThisTime, outputFifo);
        }

        readPosition += std::llround (numFramesToDo * playbackSpeedRatio);

        return true;
    }

    static constexpr int chunkSize = 128;
    const int numChannels;
    TimeStretcher timeStretcher;
    AudioFifo inputFifo { numChannels, chunkSize }, outputFifo { numChannels, chunkSize * 10 };
    double playbackSpeedRatio = 1.0, readPosition = 0.0;

    SampleCount getReadPosition() const
    {
        return static_cast<SampleCount> (std::llround (readPosition));
    }
};


//==============================================================================
class TimeRangeReader : public AudioReader
{
public:
    TimeRangeReader (std::unique_ptr<ResamplerReader> input)
        : resamplerReader (std::move (input)), source (resamplerReader.get())
    {
    }

    TimeRangeReader (std::unique_ptr<TimeStretchReader> input)
        : timeStretchSource (std::move (input)), source (timeStretchSource.get())
    {
    }

    bool read (TimeRange tr,
               choc::buffer::ChannelArrayView<float>& destBuffer)
    {
        const auto numSourceSamples = toSamples (tr.getLength(), getSampleRate());

        if (numSourceSamples == 0)
        {
            destBuffer.clear();
            return true;
        }

        const auto blockSpeedRatio = numSourceSamples / (double) destBuffer.getNumFrames();

        if (timeStretchSource)
            timeStretchSource->setSpeedAndPitch (blockSpeedRatio, 1.0f);
        else
            resamplerReader->setSpeedRatio (blockSpeedRatio);

        setPosition (tr.getStart());

        return readSamples (destBuffer);
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        return timeStretchSource ? timeStretchSource->readSamples (destBuffer)
                                 : resamplerReader->readSamples (destBuffer);
    }

    std::unique_ptr<ResamplerReader> resamplerReader;
    std::unique_ptr<TimeStretchReader> timeStretchSource;
    AudioReader* source = nullptr;
};


//==============================================================================
class TimeRangeRemappingReader : public AudioReader
{
public:
    TimeRangeRemappingReader (std::unique_ptr<TimeRangeReader> input,
                              TimeRange sourceTimeRange, double speedRatioToUse)
        : source (std::move (input)), materialTimeRange (sourceTimeRange), speedRatio (speedRatioToUse)
    {
    }

    bool read (TimeRange tr,
               choc::buffer::ChannelArrayView<float>& destBuffer)
    {
        const auto offset = toDuration (materialTimeRange.getStart());
        tr = { (tr.getStart() - offset) * speedRatio, (tr.getEnd() - offset) * speedRatio };
        return source->read (tr, destBuffer);
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        assert (false && "Use the other read method that takes a time range");
        return source->readSamples (destBuffer);
    }

    std::unique_ptr<TimeRangeReader> source;
    const TimeRange materialTimeRange;
    const double speedRatio;
};


//==============================================================================
class BeatRangeReader : public AudioReader
{
public:
    BeatRangeReader (std::unique_ptr<TimeRangeRemappingReader> input,
                     BeatRange sourceBeatRange, tempo::Sequence::Position pos)
        : source (std::move (input)), materialBeatRange (sourceBeatRange), position (pos)
    {
    }

    bool read (BeatRange br,
               choc::buffer::ChannelArrayView<float>& destBuffer)
    {
        position.set (br.getStart());
        const auto startTime = (position.getTime());
        position.set (br.getEnd());
        const auto endTime = (position.getTime());

        return source->read ({ startTime, endTime }, destBuffer);
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        return source->readSamples (destBuffer);
    }

    std::unique_ptr<TimeRangeRemappingReader> source;
    const BeatRange materialBeatRange;
    tempo::Sequence::Position position;
};


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
                    TimeRange editTime,
                    TimeDuration off,
                    TimeRange loop,
                    LiveClipLevel level,
                    double speed,
                    const juce::AudioChannelSet& channelSetToUse,
                    const juce::AudioChannelSet& destChannelsToFill,
                    ProcessState& ps,
                    EditItemID itemIDToUse,
                    bool isRendering)
   : TracktionEngineNode (ps),
     editPosition (editTime),
     loopSection (TimePosition::fromSeconds (loop.getStart().inSeconds() * speed),
                  TimePosition::fromSeconds (loop.getEnd().inSeconds() * speed)),
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

tracktion::graph::NodeProperties WaveNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.hasMidi = false;
    props.numberOfChannels = destChannels.size();
    props.nodeID = (size_t) editItemID.getRawID();

    return props;
}

void WaveNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    reader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);
    outputSampleRate = info.sampleRate;
    editPositionInSamples = tracktion::toSamples ({ editPosition.getStart(), editPosition.getEnd() }, outputSampleRate);
    updateFileSampleRate();

    const int numChannelsToUse = std::max (channelsToUse.size(), reader->getNumChannels());
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
    return editTimeToFileSample (TimePosition::fromSamples (timelinePosition, outputSampleRate));
}

int64_t WaveNode::editTimeToFileSample (TimePosition editTime) const noexcept
{
    return (int64_t) ((editTime - toDuration (editPosition.getStart() - offset)).inSeconds()
                       * originalSpeedRatio * audioFileSampleRate + 0.5);
}

bool WaveNode::updateFileSampleRate()
{
    using namespace tracktion::graph;

    if (reader == nullptr)
        return false;

    audioFileSampleRate = reader->getSampleRate();

    if (audioFileSampleRate <= 0)
        return false;

    if (! loopSection.isEmpty())
        reader->setLoopRange ({ tracktion::toSamples (loopSection.getStart(), audioFileSampleRate),
                                tracktion::toSamples (loopSection.getEnd(), audioFileSampleRate) });

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
    const auto sectionEditTime = tracktion::timeRangeFromSamples (timelineRange, outputSampleRate);

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


//==============================================================================
//==============================================================================
WaveNodeRealTime::WaveNodeRealTime (const AudioFile& af,
                                    TimeRange editTime,
                                    TimeDuration off,
                                    TimeRange loop,
                                    LiveClipLevel level,
                                    double speed,
                                    const juce::AudioChannelSet& channelSetToUse,
                                    const juce::AudioChannelSet& destChannelsToFill,
                                    ProcessState& ps,
                                    EditItemID itemIDToUse,
                                    bool isRendering)
   : TracktionEngineNode (ps),
     editPosition (editTime),
     loopSection (TimePosition::fromSeconds (loop.getStart().inSeconds() * speed),
                  TimePosition::fromSeconds (loop.getEnd().inSeconds() * speed)),
     offset (off),
     speedRatio (speed),
     editItemID (itemIDToUse),
     isOfflineRender (isRendering),
     audioFile (af),
     clipLevel (level),
     channelsToUse (channelSetToUse),
     destChannels (destChannelsToFill)
{
    // This won't work with invalid or non-existent files!
    jassert (! audioFile.isNull());

    hash_combine (stateHash, editPosition);
    hash_combine (stateHash, loopSection);
    hash_combine (stateHash, offset.inSeconds());
    hash_combine (stateHash, speedRatio);
    hash_combine (stateHash, editItemID.getRawID());
    hash_combine (stateHash, channelsToUse.size());
    hash_combine (stateHash, destChannels.size());
    hash_combine (stateHash, audioFile.getHash());
}

WaveNodeRealTime::WaveNodeRealTime (const AudioFile& af,
                                    TimeRange editTime,
                                    TimeDuration off,
                                    TimeRange loop,
                                    LiveClipLevel level,
                                    double speed,
                                    const juce::AudioChannelSet& channelSetToUse,
                                    const juce::AudioChannelSet& destChannelsToFill,
                                    ProcessState& ps,
                                    EditItemID itemIDToUse,
                                    bool isRendering,
                                    tempo::Sequence sourceFileTempoMap,
                                    SyncTempo syncTempo_,
                                    SyncPitch syncPitch_)
    : WaveNodeRealTime (af,
                        editTime,
                        off,
                        loop,
                        level,
                        speed,
                        channelSetToUse,
                        destChannelsToFill,
                        ps,
                        itemIDToUse,
                        isRendering)
{
    syncTempo = syncTempo_;
    syncPitch = syncPitch_;

    fileTempoSequence = std::make_unique<tempo::Sequence> (std::move (sourceFileTempoMap));
    tempoPosition = std::make_unique<tempo::Sequence::Position> (*fileTempoSequence);

    hash_combine (stateHash, fileTempoSequence->hash());
    hash_combine (stateHash, syncTempo);
    hash_combine (stateHash, syncPitch);
}

tracktion::graph::NodeProperties WaveNodeRealTime::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.hasMidi = false;
    props.numberOfChannels = destChannels.size();
    props.nodeID = (size_t) editItemID.getRawID();

    return props;
}

void WaveNodeRealTime::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    outputSampleRate = info.sampleRate;
    editPositionInSamples = tracktion::toSamples ({ editPosition.getStart(), editPosition.getEnd() }, outputSampleRate);

    replaceStateIfPossible (info.rootNodeToReplace);
    buildAudioReaderGraph();
}

bool WaveNodeRealTime::isReadyToProcess()
{
    // Only check this whilst rendering or it will block whilst the proxies are being created
    if (! isOfflineRender)
        return true;

    // If the hash is 0 it means an empty file path which means a missing file so
    // this will never return a valid reader and we should just bail
    if (audioFile.isNull())
        return true;

    return buildAudioReaderGraph();
}

void WaveNodeRealTime::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    assert (outputSampleRate == getSampleRate());

    //TODO: Might get a performance boost by pre-setting the file position in prepareForNextBlock
    processSection (pc, getEditTimeRange());
}

//==============================================================================
bool WaveNodeRealTime::buildAudioReaderGraph()
{
    if (audioReader != nullptr)
        return true;

    auto fileCacheReader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);

    if (fileCacheReader == nullptr)
        return false;

    auto audioFileCacheReader   = std::make_unique<AudioFileCacheReader> (std::move (fileCacheReader), isOfflineRender ? 5s : 3ms,
                                                                          destChannels, channelsToUse);
    auto loopReader             = std::make_unique<LoopReader> (std::move (audioFileCacheReader), loopSection);
    auto offsetReader           = std::make_unique<OffsetReader> (std::move (loopReader), toPosition (offset));
    auto resamplerAudioReader   = std::make_unique<ResamplerReader> (std::move (offsetReader), outputSampleRate);
    resamplerReader = resamplerAudioReader.get();
    auto timeStretchReader      = std::make_unique<TimeStretchReader> (std::move (resamplerAudioReader));
    auto timeRangeReader        = std::make_unique<TimeRangeReader> (std::move (timeStretchReader));

    if (fileTempoSequence && syncTempo == SyncTempo::yes)
    {
        auto remapperReader = std::make_unique<TimeRangeRemappingReader> (std::move (timeRangeReader), editPosition, speedRatio);
        beatRangeReader = std::make_shared<BeatRangeReader> (std::move (remapperReader), BeatRange{}, *tempoPosition);
    }
    else
    {
        audioReader = std::make_shared<TimeRangeRemappingReader> (std::move (timeRangeReader), editPosition, speedRatio);
    }

    if (! channelState)
    {
        channelState = std::make_shared<std::vector<float>>();
        const int numChannelsToUse = std::max (channelsToUse.size(), (int) (beatRangeReader ? beatRangeReader->getNumChannels() : audioReader->getNumChannels()));

        for (int i = numChannelsToUse; --i >= 0;)
            channelState->emplace_back (0.0f);
    }

    return true;
}

void WaveNodeRealTime::replaceStateIfPossible (Node* rootNodeToReplace)
{
    if (rootNodeToReplace == nullptr)
        return;

    if (stateHash == 0)
        return;

    auto visitor = [this] (Node& node)
    {
        if (auto other = dynamic_cast<WaveNodeRealTime*> (&node))
        {
            if (other->stateHash != stateHash)
                return;

            audioReader = other->audioReader;
            beatRangeReader = other->beatRangeReader;
            channelState = other->channelState;
        }
    };
    visitNodes (*rootNodeToReplace, visitor, true);
}

void WaveNodeRealTime::processSection (ProcessContext& pc, TimeRange sectionEditTime)
{
    // Check that the number of channels requested matches the destination buffer num channels
    assert (destChannels.size() == (int) pc.buffers.audio.getNumChannels());

    if ((audioReader == nullptr && beatRangeReader == nullptr)
         || sectionEditTime.getEnd() <= editPosition.getStart()
         || sectionEditTime.getStart() >= editPosition.getEnd())
        return;

    uint32_t lastSampleFadeLength = 0;

    auto destBuffer = pc.buffers.audio;
    const auto numFrames = destBuffer.getNumFrames();
    const auto numChannels = destBuffer.getNumChannels();

    // Calculate gains
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

    if (resamplerReader != nullptr)
        resamplerReader->setGains (gains[0], gains[1]);

    // Read through the audio stack
    if (beatRangeReader && beatRangeReader->read (getEditBeatRange(), pc.buffers.audio))
    {
        if (! getPlayHeadState().isContiguousWithPreviousBlock() && ! getPlayHeadState().isFirstBlockOfLoop())
            lastSampleFadeLength = std::min (numFrames, getPlayHead().isUserDragging() ? 40u : 10u);
    }
    else if (audioReader->read (sectionEditTime, pc.buffers.audio))
    {
        if (! getPlayHeadState().isContiguousWithPreviousBlock() && ! getPlayHeadState().isFirstBlockOfLoop())
            lastSampleFadeLength = std::min (numFrames, getPlayHead().isUserDragging() ? 40u : 10u);
    }
    else
    {
        lastSampleFadeLength = std::min (numFrames, 40u);
    }

    // Crossfade if a fade needs to be applied
    jassert (numChannels <= (choc::buffer::ChannelCount) channelState->size()); // this should always have been made big enough

    for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
    {
        if (channel < (choc::buffer::ChannelCount) channelState->size())
        {
            const auto dest = pc.buffers.audio.getIterator (channel).sample;

            auto& lastSample = (*channelState)[(size_t) channel];

            if (lastSampleFadeLength > 0)
            {
                for (uint32_t i = 0; i < lastSampleFadeLength; ++i)
                {
                    auto alpha = i / (float) lastSampleFadeLength;
                    dest[i] = alpha * dest[i] + lastSample * (1.0f - alpha);
                }
            }

            lastSample = dest[numFrames - 1];
        }
        else
        {
            destBuffer.getChannel (channel).clear();
        }
    }

    // Silence any samples before or after our edit time range
    // N.B. this shouldn't happen when using a clip combiner as the times should be clipped correctly
    {
        const auto timelineRange = getTimelineSampleRange();
        const auto numSamplesToClearAtStart = editPositionInSamples.getStart() - timelineRange.getStart();
        const auto numSamplesToClearAtEnd = timelineRange.getEnd() - editPositionInSamples.getEnd();

        if (numSamplesToClearAtStart > 0)
            destBuffer.getStart ((choc::buffer::FrameCount) numSamplesToClearAtStart).clear();

        if (numSamplesToClearAtEnd > 0)
            destBuffer.getEnd ((choc::buffer::FrameCount) numSamplesToClearAtEnd).clear();
    }
}

}} // namespace tracktion { inline namespace engine
