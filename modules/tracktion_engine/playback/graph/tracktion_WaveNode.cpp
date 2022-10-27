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
class AudioFileCacheReader final    : public AudioReader
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

    void setLoopRange (TimeRange loopRange)
    {
        reader->setLoopRange (toSamples (loopRange, getSampleRate()));
    }

    void reset() override
    {}

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
    void reset() override                                   { source->reset(); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        return source->readSamples (destBuffer);
    }

    std::unique_ptr<AudioReader> source;
};

//==============================================================================
class LoopReader final  : public SingleInputAudioReader
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

        if (readPos >= loopStart + loopLength)
            readPos -= loopLength;

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
            numSamplesToDo -= (int) numToRead;
        }

        return allOk;
    }

    const SampleRange loopRange;
};


//==============================================================================
class ResamplerReader : public SingleInputAudioReader
{
public:
    ResamplerReader (std::unique_ptr<AudioReader> input)
        : SingleInputAudioReader (std::move (input))
    {
    }

    /** Sets a ratio to increase or decrease playback speed. */
    virtual void setSpeedRatio (double newSpeedRatio) = 0;

    /** Sets a l/r gain to apply to channels. */
    virtual void setGains (float leftGain, float rightGain) = 0;
};


class LagrangeResamplerReader final : public ResamplerReader
{
public:
    LagrangeResamplerReader (std::unique_ptr<AudioReader> input, double sampleRateToConvertTo)
        : ResamplerReader (std::move (input)), destSampleRate (sampleRateToConvertTo)
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
    void setSpeedRatio (double newSpeedRatio) override
    {
        assert (newSpeedRatio > 0);
        speedRatio = newSpeedRatio;
    }

    /** Sets a l/r gain to apply to channels. */
    void setGains (float leftGain, float rightGain) override
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

        const auto resamplerRatio = static_cast<double> (numSourceFramesToRead) / numFrames;

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            if (channel < (choc::buffer::ChannelCount) resamplers.size())
            {
                const auto src = fileData.buffer.getReadPointer ((int) channel);
                const auto dest = destBuffer.getIterator (channel).sample;

                auto& resampler = resamplers[(size_t) channel];
                resampler.processAdding (resamplerRatio, src, dest, (int) numFrames, gains[channel & 1]);
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


class HighQualityResamplerReader final  : public ResamplerReader
{
public:
    HighQualityResamplerReader (std::unique_ptr<AudioReader> input,
                                double sampleRateToConvertTo, ResamplingQuality resamplingQuality)
        : ResamplerReader (std::move (input)),
          numChannels ((int) source->getNumChannels()),
          destSampleRate (sampleRateToConvertTo)
    {
        const int converterType = [&]
            {
                switch (resamplingQuality)
                {
                    case ResamplingQuality::sincFast:   return SRC_SINC_FASTEST;
                    case ResamplingQuality::sincMedium: return SRC_SINC_MEDIUM_QUALITY;
                    case ResamplingQuality::sincBest:   return SRC_SINC_BEST_QUALITY;
                    case ResamplingQuality::lagrange:   [[ fallthrough ]];
                    default: assert (false); return SRC_SINC_FASTEST;
                };
            }();

        int error = 0;
        src_state = src_callback_new (srcReadCallback,
                                      converterType, numChannels, &error, this);
        assert (error == 0);
    }

    ~HighQualityResamplerReader() override
    {
        src_delete (src_state);
    }

    SampleCount getPosition() override
    {
        return getReadPosition();
    }

    void setPosition (SampleCount t) override
    {
        if (std::abs (t - getReadPosition()) <= 1)
            return;

        readPosition = (double) t;

        source->setPosition (t);

        src_reset (src_state);
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, source->getSampleRate()));
    }

    double getSampleRate() override
    {
        return destSampleRate;
    }

    /** Sets a ratio to increase or decrease playback speed. */
    void setSpeedRatio (double newSpeedRatio) override
    {
        assert (newSpeedRatio > 0);
        speedRatio = newSpeedRatio;
    }

    /** Sets a l/r gain to apply to channels. */
    void setGains (float leftGain, float rightGain) override
    {
        gains[0] = leftGain;
        gains[1] = rightGain;
    }

    void reset() override
    {
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        const auto numFramesToDo = destBuffer.getNumFrames();
        choc::buffer::FrameCount startFrame = 0;
        auto framesRemaining = numFramesToDo;

        for (;;)
        {
            const auto numFramesThisChunk = std::min (framesRemaining, chunkSize);

            if (! readChunk (destBuffer.getFrameRange ({ startFrame, startFrame + numFramesThisChunk })))
                return false;

            startFrame += numFramesThisChunk;
            framesRemaining -= numFramesThisChunk;

            if (startFrame == numFramesToDo)
                break;
        }

        if (gains[0] != 1.0f || gains[1] != 1.0f)
        {
            choc::buffer::applyGain (destBuffer.getChannel (0), gains[1]);

            if (destBuffer.getNumChannels() > 1)
                choc::buffer::applyGain (destBuffer.getChannel (1), gains[1]);
        }

        return true;
    }

    static constexpr choc::buffer::FrameCount chunkSize = 1024;
    const int numChannels;
    const double destSampleRate;
    const double sourceSampleRate { source->getSampleRate() };
    const double sampleRatio { sourceSampleRate / destSampleRate  };

    SRC_STATE* src_state = nullptr;

    choc::buffer::ChannelArrayBuffer<float> scratchBuffer                   { choc::buffer::createChannelArrayBuffer (numChannels, chunkSize, [] { return 0.0f; }) };
    choc::buffer::InterleavedBuffer<float> interleavedInputScratchBuffer    { choc::buffer::createInterleavedBuffer (numChannels, chunkSize, [] { return 0.0f; }) };
    choc::buffer::InterleavedBuffer<float> interleavedOutputScratchBuffer   { interleavedInputScratchBuffer };

    double speedRatio = 1.0, readPosition = 0.0;
    float gains[2] = { 1.0f, 1.0f };

    SampleCount getReadPosition() const
    {
        return static_cast<SampleCount> (readPosition + 0.5);
    }

    static long srcReadCallback (void* data, float** destInterleavedSampleData)
    {
        return static_cast<HighQualityResamplerReader*> (data)->srcReadCallback (destInterleavedSampleData);
    }

    long srcReadCallback (float** destInterleavedSampleData)
    {
        const auto numFramesToRead = interleavedInputScratchBuffer.getSize().numFrames;

        // Read data in to temp buffer
        auto scratchView = scratchBuffer.getView();

        // If the reader fails, we can't bail out or SRC will hang so just give it an empty buffer
        if (! source->readSamples (scratchView))
            scratchView.clear();

        // Interleave
        choc::buffer::copy (interleavedInputScratchBuffer, scratchBuffer);
        *destInterleavedSampleData = interleavedInputScratchBuffer.getView().data.data;

        return static_cast<long> (numFramesToRead);
    }

    bool readChunk (const choc::buffer::ChannelArrayView<float>& destBuffer)
    {
        assert (destBuffer.getNumFrames() > 0);
        assert (destBuffer.getNumFrames() <= chunkSize);
        assert (numChannels == (int) destBuffer.getNumChannels());
        const auto numFramesToDo = destBuffer.getNumFrames();
        const auto ratio = sampleRatio * speedRatio;
        assert (ratio > 0.0);

        const auto numRead = src_callback_read (src_state, 1.0 / ratio,
                                                static_cast<long> (numFramesToDo),
                                                interleavedOutputScratchBuffer.getView().data.data);

        if (static_cast<decltype (numFramesToDo)> (numRead) != numFramesToDo)
        {
            destBuffer.clear();
            return false;
        }

        choc::buffer::copy (destBuffer, interleavedOutputScratchBuffer.getStart (numFramesToDo));
        readPosition += numFramesToDo * ratio;

        return true;
    }
};


#if USE_RESET_TIMESTRETCH
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
        outputFifo.setSize (numChannels, timeStretcher.getMaxFramesNeeded());
    }

    SampleCount getPosition() override
    {
        return getReadPosition();
    }

    void setPosition (SampleCount t) override
    {
        nextReadPos = t;
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    void reset() override
    {
        source->setPosition (nextReadPos);
        timeStretcher.reset();
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
        inputFifo.reset();
        outputFifo.reset();
    }

    void setSpeed (double speedRatio)
    {
        if (playbackSpeedRatio == speedRatio)
            return;

        playbackSpeedRatio = speedRatio;
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
    }

    void setPitch (double semitones)
    {
        if (semitonesShift == semitones)
            return;

        semitonesShift = semitones;
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
    }

    void setSpeedAndPitch (double speedRatio, double semitones)
    {
        playbackSpeedRatio = speedRatio;
        semitonesShift = semitones;
        [[ maybe_unused ]] const bool ok = timeStretcher.setSpeedAndPitch ((float) (1.0 / speedRatio), (float) semitonesShift);
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

        readPosition += numFramesToDo * playbackSpeedRatio;

        return true;
    }

    static constexpr int chunkSize = 1024;
    const int numChannels;
    TimeStretcher timeStretcher;
    AudioFifo inputFifo { numChannels, chunkSize }, outputFifo { numChannels, chunkSize };
    double playbackSpeedRatio = 1.0, semitonesShift = 0.0, readPosition = 0.0;
    SampleCount nextReadPos = 0;

    SampleCount getReadPosition() const
    {
        return static_cast<SampleCount> (std::llround (readPosition));
    }
};
#else
class TimeStretchReader final   : public SingleInputAudioReader
{
public:
    TimeStretchReader (std::unique_ptr<AudioReader> input)
        : SingleInputAudioReader (std::move (input)), numChannels ((int) source->getNumChannels())
    {
        timeStretcher.initialise (source->getSampleRate(), chunkSize, numChannels,
                                  TimeStretcher::defaultMode, {}, true);
        inputFifo.setSize (numChannels, timeStretcher.getMaxFramesNeeded());
        outputFifo.setSize (numChannels, timeStretcher.getMaxFramesNeeded());
    }

    SampleCount getPosition() override
    {
        return getReadPosition();
    }

    void setPosition (SampleCount t) override
    {
        if (std::abs (t - getReadPosition()) <= 1)
            return;

        readPosition = (double) t;

        source->setPosition (t);
        timeStretcher.reset();
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
        inputFifo.reset();
        outputFifo.reset();
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    void reset() override
    {
    }

    void setSpeed (double speedRatio)
    {
        if (playbackSpeedRatio == speedRatio)
            return;

        playbackSpeedRatio = speedRatio;
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
    }

    void setPitch (double semitones)
    {
        if (semitonesShift == semitones)
            return;

        semitonesShift = semitones;
        setSpeedAndPitch (playbackSpeedRatio, semitonesShift);
    }

    void setSpeedAndPitch (double speedRatio, double semitones)
    {
        playbackSpeedRatio = speedRatio;
        semitonesShift = semitones;
        [[ maybe_unused ]] const bool ok = timeStretcher.setSpeedAndPitch ((float) (1.0 / speedRatio), (float) semitonesShift);
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

        readPosition += numFramesToDo * playbackSpeedRatio;

        return true;
    }

    static constexpr int chunkSize = 1024;
    const int numChannels;
    TimeStretcher timeStretcher;
    AudioFifo inputFifo { numChannels, chunkSize }, outputFifo { numChannels, chunkSize };
    double playbackSpeedRatio = 1.0, semitonesShift = 0.0, readPosition = std::numeric_limits<double>::lowest();

    SampleCount getReadPosition() const
    {
        return static_cast<SampleCount> (readPosition + 0.5);
    }
};
#endif

struct WarpedTime
{
    TimePosition position;
    double stretchRatio = 1.0;
};

inline WarpedTime warpTime (const WarpMap& map, TimePosition time)
{
    if (map.empty())
        return { time, 1.0 };

    assert (map.size() > (size_t) 1);
    WarpPoint startMarker, endMarker;

    auto first = map.front();
    auto last  = map.back();

    if (time <= first.warpTime) //below or on the 1st marker
    {
        const auto durationBefore = time - first.warpTime;
        return { toPosition (-durationBefore), 1.0 };
    }
    else if (time > last.warpTime) // after the last marker
    {
        const auto durationBeyondEnd = time - last.warpTime;
        return { last.sourceTime + durationBeyondEnd, 1.0 };
    }
    else
    {
        size_t index = 0;
        auto numMarkers = map.size();

        while (index < numMarkers && map[index].warpTime < time)
            index++;

        if (index > 0)
            startMarker = map[index - 1];

        endMarker = map[index];
    }

    const auto sourceDuration = endMarker.sourceTime - startMarker.sourceTime;
    const auto warpedDuration = endMarker.warpTime - startMarker.warpTime;

    TimePosition sourcePosition;

    if (warpedDuration == 0.0s)
        return { 0s, 1.0 };

    const double warpProportion = (time - startMarker.warpTime) / warpedDuration;
    sourcePosition = startMarker.sourceTime + (sourceDuration * warpProportion);
    const double ratio = sourceDuration / warpedDuration;

    return { sourcePosition, ratio };
}

//==============================================================================
class WarpReader final  : public SingleInputAudioReader
{
public:
    WarpReader (std::unique_ptr<AudioReader> input,
                WarpMap warpMap)
        : SingleInputAudioReader (std::make_unique<TimeStretchReader> (std::move (input))),
          reader (static_cast<TimeStretchReader*> (source.get())), map (warpMap)
    {
    }

    SampleCount getPosition() override
    {
        return readPosition;
    }

    void setPosition (TimePosition t) override
    {
        setPosition (toSamples (t, getSampleRate()));
    }

    void setPosition (SampleCount t) override
    {
        if (t == readPosition)
            return;

        readPosition = t;
        setSourcePosition (t);
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        const auto unwarpedStartTime = TimePosition::fromSamples (readPosition, getSampleRate());
        const auto ratio = warpTime (map, unwarpedStartTime).stretchRatio;

        reader->setSpeed (ratio);
        readPosition += (SampleCount) destBuffer.getNumFrames();

        return reader->readSamples (destBuffer);
    }

private:
    TimeStretchReader* reader = nullptr;
    WarpMap map;
    SampleCount readPosition = 0;

    void setSourcePosition (SampleCount pos)
    {
        const auto sampleRate = getSampleRate();
        const auto sourceTime = TimePosition::fromSamples (pos, sampleRate);
        const auto warpedTime = warpTime (map, sourceTime).position;
        const auto warpedSamplePos = toSamples (warpedTime, sampleRate);
        source->setPosition (warpedSamplePos);
    }
};


//==============================================================================
class PitchAdjustReader final   : public SingleInputAudioReader
{
public:
    PitchAdjustReader (std::unique_ptr<AudioReader> input,
                       TimeStretchReader* timeStretcher,
                       const tempo::Sequence& fileTempoSequence)
        : SingleInputAudioReader (std::move (input)),
          timeStretchSource (timeStretcher),
          rootPitch (fileTempoSequence.getKeyAt (0s).pitch),
          syncToKey (true)
    {
        assert (timeStretchSource != nullptr);
    }

    PitchAdjustReader (std::unique_ptr<AudioReader> input,
                       TimeStretchReader* timeStretcher,
                       float numSemitones)
        : SingleInputAudioReader (std::move (input)),
          timeStretchSource (timeStretcher),
          numSemitonesShift (numSemitones)
    {
        assert (timeStretchSource != nullptr);
    }

    void setKey (tempo::Key newKey)
    {
        key = newKey;
    }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        if (syncToKey)
        {
            int pitch = key.pitch;
            int transposeBase = pitch - rootPitch;

            while (transposeBase > 6)  transposeBase -= 12;
            while (transposeBase < -6) transposeBase += 12;

            timeStretchSource->setPitch (static_cast<double> (transposeBase));
        }
        else
        {
            timeStretchSource->setPitch (static_cast<double> (numSemitonesShift));
        }

        return SingleInputAudioReader::readSamples (destBuffer);
    }

    TimeStretchReader* timeStretchSource;
    const int rootPitch = 60;
    tempo::Key key;

    bool syncToKey = false;
    float numSemitonesShift = 0.0f;
};


//==============================================================================
class TimeRangeReader final : public SingleInputAudioReader
{
public:
    TimeRangeReader (std::unique_ptr<ResamplerReader> input)
        : SingleInputAudioReader (std::move (input)),
          resamplerReader (static_cast<ResamplerReader*> (source.get()))
    {
    }

    TimeRangeReader (std::unique_ptr<TimeStretchReader> input)
        : SingleInputAudioReader (std::move (input)),
          timeStretchSource (static_cast<TimeStretchReader*> (source.get()))
    {
    }

    TimeRangeReader (std::unique_ptr<AudioReader> input,
                     TimeStretchReader* timeStretcher)
        : SingleInputAudioReader (std::move (input)),
          timeStretchSource (timeStretcher)
    {
        assert (timeStretcher != nullptr);
    }

    bool read (TimeRange tr,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               TimeDuration editDuration,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        const auto numSourceSamples = toSamples (tr.getLength(), getSampleRate());

        if (numSourceSamples == 0)
        {
            destBuffer.clear();
            return true;
        }

        const auto blockSpeedRatio = (tr.getLength() / editDuration) * playbackSpeedRatio;

        if (timeStretchSource)
            timeStretchSource->setSpeed (blockSpeedRatio);
        else
            resamplerReader->setSpeedRatio (blockSpeedRatio);

        setPosition (tr.getStart());

        if (! isContiguous)
            reset();

        return readSamples (destBuffer);
    }

    ResamplerReader* resamplerReader = nullptr;
    TimeStretchReader* timeStretchSource = nullptr;
};


//==============================================================================
class EditToClipTimeReader final    : public AudioReader
{
public:
    EditToClipTimeReader (std::unique_ptr<TimeRangeReader> input,
                          TimeRange sourceTimeRange, TimeDuration offsetTime,
                          double speedRatioToUse)
        : source (std::move (input)),
          clipPosition (sourceTimeRange), offset (offsetTime),
          speedRatio (speedRatioToUse)
    {
    }

    bool read (TimeRange editTimeRange,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               TimeDuration editDuration,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        const auto clipStartOffset = toDuration (clipPosition.getStart());
        TimeRange tr ((editTimeRange.getStart() - clipStartOffset) * speedRatio,
                      (editTimeRange.getEnd() - clipStartOffset) * speedRatio);
        tr = tr + (offset * speedRatio);
        const auto readOk = source->read (tr, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

        // Clear samples outside of clip position
        // N.B. this shouldn't happen when using a clip combiner as the times should be clipped correctly
        if (! tr.isEmpty())
        {
            using choc::buffer::FrameCount;
            const auto timeToClearAtStart  = editTimeRange.contains (clipPosition.getStart()) ? clipPosition.getStart() - editTimeRange.getStart() : 0_td;
            const auto timeToClearAtEnd    = editTimeRange.contains (clipPosition.getEnd())   ? editTimeRange.getEnd() - clipPosition.getEnd()     : 0_td;

            const auto editTimeRangeLength = editTimeRange.getLength();
            const auto numFrames = destBuffer.getNumFrames();

            if (timeToClearAtStart > 0_td)
            {
                const auto numSamplesToClearAtStart = static_cast<FrameCount> (numFrames * (timeToClearAtStart / editTimeRangeLength) + 0.5);
                destBuffer.getStart (numSamplesToClearAtStart).clear();
            }

            if (timeToClearAtEnd > 0_td)
            {
                const auto numSamplesToClearAtEnd = static_cast<FrameCount> (numFrames * (timeToClearAtEnd / editTimeRangeLength) + 0.5);
                destBuffer.getEnd (numSamplesToClearAtEnd).clear();
            }
        }

        return readOk;
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    void reset() override                                   { source->reset(); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        assert (false && "Use the other read method that takes a time range");
        return source->readSamples (destBuffer);
    }

    std::unique_ptr<TimeRangeReader> source;
    const TimeRange clipPosition;
    const TimeDuration offset;
    const double speedRatio;
};


//==============================================================================
/** N.B. This has to assume a constant Edit tempo per block.
    The top level Edit player should chunk at tempo changes.
*/
class BeatRangeReader final : public AudioReader
{
public:
    BeatRangeReader (std::unique_ptr<TimeRangeReader> input,
                     BeatRange loopRange_,
                     BeatDuration offset_,
                     tempo::Sequence::Position sourceSequencePosition_)
        : source (std::move (input)),
          loopRange (loopRange_), offset (offset_),
          sourceSequencePosition (sourceSequencePosition_)
    {
    }

    bool read (BeatRange br,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               TimeDuration editDuration,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        // Apply offset first
        br = br + offset;

        // First apply the looping
        return readLoopedBeatRange (br, destBuffer, editDuration, isContiguous, playbackSpeedRatio);
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    void reset() override                                   { source->reset(); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        return source->readSamples (destBuffer);
    }

private:
    std::unique_ptr<TimeRangeReader> source;
    const BeatRange loopRange;
    const BeatDuration offset;
    tempo::Sequence::Position sourceSequencePosition;

    bool readLoopedBeatRange (BeatRange br,
                              choc::buffer::ChannelArrayView<float>& destBuffer,
                              TimeDuration editDuration,
                              bool isContiguous,
                              double playbackSpeedRatio)
    {
        using choc::buffer::FrameCount;

        if (loopRange.isEmpty())
            return readBeatRange (br, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

        const auto s = linearPositionToLoopPosition (br.getStart(), loopRange);
        const auto e = linearPositionToLoopPosition (br.getEnd(), loopRange);

        if (s > e)
        {
            if (s >= loopRange.getEnd())
                return readBeatRange ({ loopRange.getStart(), e }, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

            if (e <= loopRange.getStart())
                return readBeatRange ({ s, loopRange.getEnd() }, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

            // Otherwise range is split
            const BeatRange br1 (s, loopRange.getEnd());
            const BeatRange br2 (loopRange.getStart(), e);
            const auto prop1 = br1.getLength() / br.getLength();
            const auto prop2 = 1.0 - prop1;

            const auto numFrames = destBuffer.getNumFrames();
            const auto numFrames1 = static_cast<FrameCount> (std::llround (numFrames * prop1));

            auto buffer1 = destBuffer.getStart (numFrames1);
            auto buffer2 = destBuffer.getFrameRange ({ numFrames1, numFrames });

            return readBeatRange (br1, buffer1, editDuration * prop1, isContiguous, playbackSpeedRatio)
                && readBeatRange (br2, buffer2, editDuration * prop2, false, playbackSpeedRatio);
        }

        return readBeatRange ({ s, e }, destBuffer, editDuration, isContiguous, playbackSpeedRatio);
    }

    bool readBeatRange (BeatRange br,
                        choc::buffer::ChannelArrayView<float>& destBuffer,
                        TimeDuration editDuration,
                        bool isContiguous,
                        double playbackSpeedRatio)
    {
        // Convert source beat range to source time range
        sourceSequencePosition.set (br.getStart());
        const auto startTime = (sourceSequencePosition.getTime());
        sourceSequencePosition.set (br.getEnd());
        const auto endTime = (sourceSequencePosition.getTime());

        return source->read ({ startTime, endTime }, destBuffer, editDuration, isContiguous, playbackSpeedRatio);
    }

    static inline BeatPosition linearPositionToLoopPosition (BeatPosition position, BeatRange loopRange)
    {
        return loopRange.getStart() + BeatDuration::fromBeats (std::fmod (position.inBeats(), loopRange.getLength().inBeats()));
    }
};

//==============================================================================
class EditToClipBeatReader final    : public AudioReader
{
public:
    EditToClipBeatReader (std::unique_ptr<BeatRangeReader> input, BeatRange clipPosition_)
        : source (std::move (input)), clipPosition (clipPosition_)
    {
    }

    bool read (BeatRange editBeatRange,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               TimeDuration editDuration,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        const auto clipBeatRange = editBeatRange - toDuration (clipPosition.getStart());
        const auto readOk = source->read (clipBeatRange, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

        // Clear samples outside of clip position
        // N.B. this shouldn't happen when using a clip combiner as the times should be clipped correctly
        if (! editBeatRange.isEmpty())
        {
            using choc::buffer::FrameCount;
            const auto beatsToClearAtStart  = clipPosition.getStart() - editBeatRange.getStart();
            const auto beatsToClearAtEnd    = editBeatRange.getEnd() - clipPosition.getEnd();

            const auto editBeatRangeLength = editBeatRange.getLength();
            const auto numFrames = destBuffer.getNumFrames();

            if (beatsToClearAtStart > 0_bd)
            {
                const auto numSamplesToClearAtStart = static_cast<FrameCount> (numFrames * (beatsToClearAtStart / editBeatRangeLength) + 0.5);
                destBuffer.getStart (numSamplesToClearAtStart).clear();
            }

            if (beatsToClearAtEnd > 0_bd)
            {
                const auto numSamplesToClearAtEnd = static_cast<FrameCount> (numFrames * (beatsToClearAtEnd / editBeatRangeLength) + 0.5);
                destBuffer.getEnd (numSamplesToClearAtEnd).clear();
            }
        }

        return readOk;
    }

    choc::buffer::ChannelCount getNumChannels() override    { return source->getNumChannels(); }
    SampleCount getPosition() override                      { return source->getPosition(); }
    void setPosition (SampleCount t) override               { source->setPosition (t); }
    void setPosition (TimePosition t) override              { source->setPosition (t); }
    void reset() override                                   { source->reset(); }
    double getSampleRate() override                         { return source->getSampleRate(); }

    bool readSamples (choc::buffer::ChannelArrayView<float>& destBuffer) override
    {
        jassertfalse;
        return source->readSamples (destBuffer);
    }

private:
    std::unique_ptr<BeatRangeReader> source;
    const BeatRange clipPosition;
};


//==============================================================================
class EditReader
{
public:
    EditReader (std::unique_ptr<EditToClipBeatReader> beatReader,
                std::unique_ptr<EditToClipTimeReader> timeReader)
        : editToClipBeatReader (std::move (beatReader)),
          editToClipTimeReader (std::move (timeReader))
    {
        assert ((editToClipBeatReader || editToClipTimeReader) && "Must supply one valid reader");
    }

    bool isBeatBased() const            { return editToClipBeatReader != nullptr; }
    bool isTimeBased() const            { return editToClipTimeReader != nullptr; }

    choc::buffer::ChannelCount getNumChannels() const
    {
        return editToClipBeatReader ? editToClipBeatReader->getNumChannels()
                                    : editToClipTimeReader->getNumChannels();
    }

    bool read (BeatRange editBeatRange,
               TimeRange editTimeRange,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        const auto editDuration = editTimeRange.getLength();

        if (editToClipBeatReader)
            return editToClipBeatReader->read (editBeatRange, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

        if (editToClipTimeReader)
            return editToClipTimeReader->read (editTimeRange, destBuffer, editDuration, isContiguous, playbackSpeedRatio);

        return false;
    }

private:
    std::unique_ptr<EditToClipBeatReader> editToClipBeatReader;
    std::unique_ptr<EditToClipTimeReader> editToClipTimeReader;
};

//==============================================================================
class SpeedFadeEditReader
{
public:
    SpeedFadeEditReader (std::unique_ptr<EditReader> editReader,
                         SpeedFadeDescription speedFadeDesc,
                         std::optional<tempo::Sequence::Position> editTempoPosition)
        : reader (std::move (editReader)),
          fadeDesc (speedFadeDesc),
          tempoPosition (std::move (editTempoPosition))
    {
        for (int i = (int) getNumChannels(); --i >= 0;)
        {
            resamplers.emplace_back (juce::LagrangeInterpolator());
            resamplers.back().reset();
        }
    }

    bool isBeatBased() const            { return reader->isBeatBased(); }
    bool isTimeBased() const            { return reader->isTimeBased(); }

    choc::buffer::ChannelCount getNumChannels() const
    {
        return reader->getNumChannels();
    }

    bool read (BeatRange editBeatRange,
               TimeRange editTimeRange,
               choc::buffer::ChannelArrayView<float>& destBuffer,
               bool isContiguous,
               double playbackSpeedRatio)
    {
        if (! shouldWarp() || editTimeRange.isEmpty())
            return  reader->read (editBeatRange, editTimeRange, destBuffer, isContiguous, playbackSpeedRatio);

        const auto originalDuration = editTimeRange.getLength();
        std::tie (editBeatRange, editTimeRange) = warpTimeRanges (editBeatRange, editTimeRange);

        const auto editDuration = editTimeRange.getLength();
        const auto ratio = editDuration / originalDuration;

        const auto numChannels = destBuffer.getNumChannels();
        assert (numChannels <= (choc::buffer::ChannelCount) resamplers.size());
        assert (destBuffer.getNumChannels() == numChannels);

        const auto numFrames = destBuffer.getNumFrames();
        const int numSourceFramesToRead = static_cast<int> ((numFrames * ratio) + 0.5);
        AudioScratchBuffer sourceData ((int) numChannels, numSourceFramesToRead);
        auto sourceDataView = toBufferView (sourceData.buffer);
        sourceDataView.clear();
        const bool ok = reader->read (editBeatRange, editTimeRange, sourceDataView, isContiguous, playbackSpeedRatio);

        const auto resamplerRatio = static_cast<double> (numSourceFramesToRead) / numFrames;

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            if (channel < (choc::buffer::ChannelCount) resamplers.size())
            {
                const auto src = sourceData.buffer.getReadPointer ((int) channel);
                const auto dest = destBuffer.getIterator (channel).sample;

                auto& resampler = resamplers[(size_t) channel];
                resampler.process (resamplerRatio, src, dest, (int) numFrames);
            }
            else
            {
                destBuffer.getChannel (channel).clear();
            }
        }

        return ok;
    }

private:
    std::unique_ptr<EditReader> reader;

    SpeedFadeDescription fadeDesc;
    std::optional<tempo::Sequence::Position> tempoPosition;
    std::vector<juce::LagrangeInterpolator> resamplers;

    bool shouldWarp() const
    {
        return tempoPosition && ! fadeDesc.isEmpty();
    }

    std::pair<BeatRange, TimeRange> warpTimeRanges (BeatRange br, TimeRange tr)
    {
        assert (shouldWarp());
        tr = { warpTimePosition (tr.getStart()),
               warpTimePosition (tr.getEnd()) };

        tempoPosition->set (tr.getStart());
        const auto startBeat = tempoPosition->getBeats();
        tempoPosition->set (tr.getEnd());
        const auto endBeat = tempoPosition->getBeats();
        br = { startBeat, endBeat };

        return { br, tr };
    }

    TimePosition warpTimePosition (TimePosition tp)
    {
        if (! fadeDesc.inTimeRange.isEmpty()
            && fadeDesc.inTimeRange.containsInclusive (tp))
        {
            const auto prop = (tp - fadeDesc.inTimeRange.getStart()) / fadeDesc.inTimeRange.getLength();
            const auto newProp = rescale (fadeDesc.fadeInType, prop, true);
            return fadeDesc.inTimeRange.getStart() + (fadeDesc.inTimeRange.getLength() * newProp);
        }
        else if (! fadeDesc.outTimeRange.isEmpty()
                 && fadeDesc.outTimeRange.containsInclusive (tp))
        {
            const auto prop = (tp - fadeDesc.outTimeRange.getStart()) / fadeDesc.outTimeRange.getLength();
            const auto newProp = rescale (fadeDesc.fadeOutType, prop, false);
            return fadeDesc.outTimeRange.getStart() + (fadeDesc.outTimeRange.getLength() * newProp);
        }

        return tp;
    }

    static double rescale (AudioFadeCurve::Type t, double proportion, bool rampUp)
    {
        switch (t)
        {
            case AudioFadeCurve::convex:
                return rampUp ? (-2.0 * std::cos ((juce::MathConstants<double>::pi * proportion) / 2.0)) / juce::MathConstants<double>::pi + 1.0
                              : 1.0 - ((-2.0 * std::cos ((juce::MathConstants<double>::pi * (proportion - 1.0)) / 2.0)) / juce::MathConstants<double>::pi + 1.0);

            case AudioFadeCurve::concave:
                return rampUp ? proportion - (2.0 * std::sin ((juce::MathConstants<double>::pi * proportion) / 2.0)) / juce::MathConstants<double>::pi + (2.0 / juce::MathConstants<double>::pi)
                              : ((2.0 * std::sin ((juce::MathConstants<double>::pi * (proportion + 1.0)) / 2.0)) / juce::MathConstants<double>::pi) + proportion - (2.0 / juce::MathConstants<double>::pi);

            case AudioFadeCurve::sCurve:
                return rampUp ? (proportion / 2.0) - (std::sin (juce::MathConstants<double>::pi * proportion) / (2.0 * juce::MathConstants<double>::pi)) + 0.5
                              : std::sin (juce::MathConstants<double>::pi * proportion) / (2.0 * juce::MathConstants<double>::pi) + (proportion / 2.0);

            case AudioFadeCurve::linear:
            default:
                return rampUp ? (juce::square (proportion) * 0.5) + 0.5
                              : ((-juce::square (proportion - 1.0)) * 0.5) + 0.5;
        }
    }
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

    const int numChannelsToUse = std::max (channelsToUse.size(), reader != nullptr ? reader->getNumChannels() : 0);
    replaceChannelStateIfPossible (info.nodeGraphToReplace, numChannelsToUse);

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

void WaveNode::replaceChannelStateIfPossible (NodeGraph* nodeGraphToReplace, int numChannelsToUse)
{
    const auto nodeID = (size_t) editItemID.getRawID();
    assert (getNodeProperties().nodeID == nodeID);

    if (auto oldWaveNode = findNodeWithIDIfNonZero<WaveNode> (nodeGraphToReplace, nodeID))
        replaceChannelStateIfPossible (*oldWaveNode, numChannelsToUse);
}

void WaveNode::replaceChannelStateIfPossible (WaveNode& other, int numChannelsToUse)
{
    if (other.editItemID != editItemID)
        return;

    if (! other.channelState)
        return;

    if (other.channelState->size() == numChannelsToUse)
        channelState = other.channelState;
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

    if (numFileSamples > 0)
    {
        SCOPED_REALTIME_CHECK

        if (reader->readSamples (numFileSamples + 2, fileData.buffer, destBufferChannels, 0,
                                 channelsToUse,
                                 isOfflineRender ? 5000 : 3))
        {
            if (! getPlayHeadState().isContiguousWithPreviousBlock() && ! getPlayHeadState().isFirstBlockOfLoop())
                lastSampleFadeLength = std::min (numFrames, 40u);
        }
        else
        {
            lastSampleFadeLength = std::min (numFrames, 40u);
            fileData.buffer.clear();
        }
    }
    else
    {
        lastSampleFadeLength = 40u;

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            if (channel >= (choc::buffer::ChannelCount) channelState->size())
                continue;

            auto& state = *channelState->getUnchecked ((int) channel);

            if (state.lastSample == 0.0)
                continue;
            
            const auto dest = destBuffer.getIterator (channel).sample;

            for (uint32_t i = 0; i < lastSampleFadeLength; ++i)
            {
                auto alpha = i / (float) lastSampleFadeLength;
                dest[i] = state.lastSample * (1.0f - alpha);
            }
        }

        for (auto state : *channelState)
        {
            state->resampler.reset();
            state->lastSample = 0.0;
        }

        return;
    }

    auto ratio = numFileSamples / (double) numFrames;

    if (ratio <= 0.0)
        return;

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
        auto numSamplesToClearAtStart = std::min (editPositionInSamples.getStart() - timelineRange.getStart(), (SampleCount) destBuffer.getNumFrames());
        auto numSamplesToClearAtEnd = std::min (timelineRange.getEnd() - editPositionInSamples.getEnd(), (SampleCount) destBuffer.getNumFrames());

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
                                    bool isRendering,
                                    ResamplingQuality resamplingQualityToUse,
                                    SpeedFadeDescription speedDesc,
                                    std::optional<tempo::Sequence::Position> editTempoSeq,
                                    TimeStretcher::Mode mode,
                                    TimeStretcher::ElastiqueProOptions options,
                                    float pitchChange)
    : TracktionEngineNode (ps),
      editPositionTime (editTime),
      loopSectionTime (loop.rescaled (loop.getStart(), speed)),
      offsetTime (off),
      speedRatio (speed),
      editItemID (itemIDToUse),
      isOfflineRender (isRendering),
      resamplingQuality (resamplingQualityToUse),
      audioFile (af),
      speedFadeDescription (std::move (speedDesc)),
      editTempoSequence (std::move (editTempoSeq)),
      timeStretcherMode (mode),
      elastiqueProOptions (options),
      clipLevel (level),
      channelsToUse (channelSetToUse),
      destChannels (destChannelsToFill),
      pitchChangeSemitones (pitchChange)
{
    // This won't work with invalid or non-existent files!
    jassert (! audioFile.isNull());

    hash_combine (stateHash, editPositionTime);
    hash_combine (stateHash, loopSectionTime);
    hash_combine (stateHash, offsetTime.inSeconds());
    hash_combine (stateHash, speedRatio);
    hash_combine (stateHash, editItemID.getRawID());
    hash_combine (stateHash, channelsToUse.size());
    hash_combine (stateHash, destChannels.size());
    hash_combine (stateHash, audioFile.getHash());
    hash_combine (stateHash, resamplingQualityToUse);
    hash_combine (stateHash, speedFadeDescription);
    hash_combine (stateHash, static_cast<int> (timeStretcherMode));
    hash_combine (stateHash, elastiqueProOptions.toString().hashCode());
    hash_combine (stateHash, pitchChangeSemitones);
}

WaveNodeRealTime::WaveNodeRealTime (const AudioFile& af,
                                    TimeStretcher::Mode mode,
                                    TimeStretcher::ElastiqueProOptions options,
                                    BeatRange editTime,
                                    BeatDuration off,
                                    BeatRange loop,
                                    LiveClipLevel level,
                                    const juce::AudioChannelSet& channelSetToUse,
                                    const juce::AudioChannelSet& destChannelsToFill,
                                    ProcessState& ps,
                                    EditItemID itemIDToUse,
                                    bool isRendering,
                                    ResamplingQuality resamplingQualityToUse,
                                    SpeedFadeDescription speedDesc,
                                    std::optional<tempo::Sequence::Position> editTempoSeq,
                                    std::optional<WarpMap> warp,
                                    tempo::Sequence sourceFileTempoMap,
                                    SyncTempo syncTempo_,
                                    SyncPitch syncPitch_,
                                    std::optional<tempo::Sequence> chordPitchSequence_)
    : TracktionEngineNode (ps),
      editPositionBeats (editTime),
      loopSectionBeats (loop),
      offsetBeats (off),
      editItemID (itemIDToUse),
      isOfflineRender (isRendering),
      resamplingQuality (resamplingQualityToUse),
      audioFile (af),
      speedFadeDescription (std::move (speedDesc)),
      editTempoSequence (std::move (editTempoSeq)),
      warpMap (std::move (warp)),
      timeStretcherMode (mode),
      elastiqueProOptions (options),
      clipLevel (level),
      channelsToUse (channelSetToUse),
      destChannels (destChannelsToFill)
{
    syncTempo = syncTempo_;
    syncPitch = syncPitch_;

    fileTempoSequence = std::make_shared<tempo::Sequence> (std::move (sourceFileTempoMap));
    fileTempoPosition = std::make_shared<tempo::Sequence::Position> (*fileTempoSequence);

    if (chordPitchSequence_)
    {
        chordPitchSequence = std::make_shared<tempo::Sequence> (*chordPitchSequence_);
        chordPitchPosition = std::make_shared<tempo::Sequence::Position> (*chordPitchSequence);
    }

    // This won't work with invalid or non-existent files!
    jassert (! audioFile.isNull());

    hash_combine (stateHash, editPositionBeats);
    hash_combine (stateHash, loopSectionBeats);
    hash_combine (stateHash, offsetBeats.inBeats());
    hash_combine (stateHash, editItemID.getRawID());
    hash_combine (stateHash, channelsToUse.size());
    hash_combine (stateHash, destChannels.size());
    hash_combine (stateHash, audioFile.getHash());
    hash_combine (stateHash, resamplingQualityToUse);
    hash_combine (stateHash, speedFadeDescription);

    if (warpMap)
        hash_combine (stateHash, *warpMap);

    hash_combine (stateHash, static_cast<int> (timeStretcherMode));
    hash_combine (stateHash, elastiqueProOptions.toString().hashCode());

    hash_combine (stateHash, fileTempoSequence->hash());
    hash_combine (stateHash, syncTempo);
    hash_combine (stateHash, syncPitch);

    if (chordPitchSequence)
        hash_combine (stateHash, chordPitchSequence->hash());
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

    replaceStateIfPossible (info.nodeGraphToReplace);
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
    processSection (pc);
}

//==============================================================================
bool WaveNodeRealTime::buildAudioReaderGraph()
{
    if (editReader)
        return true;

    auto fileCacheReader = audioFile.engine->getAudioFileManager().cache.createReader (audioFile);

    if (fileCacheReader == nullptr || fileCacheReader->getSampleRate() == 0.0)
        return false;

    std::unique_ptr<AudioReader> loopReader = std::make_unique<AudioFileCacheReader> (std::move (fileCacheReader), isOfflineRender ? 5s : 3ms,
                                                                                      destChannels, channelsToUse);

    if (warpMap)
        loopReader = std::make_unique<WarpReader> (std::move (loopReader), std::move (*warpMap));

    if (! loopSectionTime.isEmpty())
        loopReader = std::make_unique<LoopReader> (std::move (loopReader), loopSectionTime);

    const bool timestretchDisabled = timeStretcherMode == TimeStretcher::Mode::disabled;
    std::unique_ptr<ResamplerReader> resamplerAudioReader;

    if (resamplingQuality == ResamplingQuality::lagrange)
        resamplerAudioReader    = std::make_unique<LagrangeResamplerReader> (std::move (loopReader), outputSampleRate);
    else
        resamplerAudioReader    = std::make_unique<HighQualityResamplerReader> (std::move (loopReader), outputSampleRate, resamplingQuality);

    resamplerReader = resamplerAudioReader.get();
    std::unique_ptr<TimeStretchReader> timeStretchReader;

    if (! timestretchDisabled)
        timeStretchReader = std::make_unique<TimeStretchReader> (std::move (resamplerAudioReader));

    auto timeStretcher = timeStretchReader.get();
    std::unique_ptr<TimeRangeReader> timeRangeReader;
    std::unique_ptr<EditReader> basicEditReader;

    if (syncPitch == SyncPitch::yes)
    {
        assert (fileTempoSequence);
        auto pitchAdjuster = std::make_unique<PitchAdjustReader> (std::move (timeStretchReader), timeStretchReader.get(), *fileTempoSequence);
        pitchAdjustReader  = pitchAdjuster.get();
        timeRangeReader    = std::make_unique<TimeRangeReader> (std::move (pitchAdjuster), timeStretcher);
    }
    else
    {
        if (timestretchDisabled)
        {
            timeRangeReader    = std::make_unique<TimeRangeReader> (std::move (resamplerAudioReader));
        }
        else
        {
            if (pitchChangeSemitones != 0.0f)
            {
                auto pitchAdjuster = std::make_unique<PitchAdjustReader> (std::move (timeStretchReader), timeStretchReader.get(), pitchChangeSemitones);
                pitchAdjustReader  = pitchAdjuster.get();
                timeRangeReader    = std::make_unique<TimeRangeReader> (std::move (pitchAdjuster), timeStretcher);
            }
            else
            {
                timeRangeReader    = std::make_unique<TimeRangeReader> (std::move (timeStretchReader), timeStretcher);
            }
        }
    }

    if (syncTempo == SyncTempo::yes || syncPitch == SyncPitch::yes)
    {
        assert (fileTempoSequence);
        auto beatRangeReader    = std::make_unique<BeatRangeReader> (std::move (timeRangeReader),
                                                                     loopSectionBeats, offsetBeats, *fileTempoPosition);
        auto editToClipBeatReader    = std::make_unique<EditToClipBeatReader> (std::move (beatRangeReader), editPositionBeats);
        basicEditReader = std::make_unique<EditReader> (std::move (editToClipBeatReader), nullptr);
    }
    else
    {
        auto editToClipTimeReader = std::make_unique<EditToClipTimeReader> (std::move (timeRangeReader), editPositionTime, offsetTime, speedRatio);
        basicEditReader = std::make_unique<EditReader> (nullptr, std::move (editToClipTimeReader));
    }

    editReader = std::make_shared<SpeedFadeEditReader> (std::move (basicEditReader), speedFadeDescription, editTempoSequence);

    if (! channelState)
    {
        channelState = std::make_shared<std::vector<float>>();
        const int numChannelsToUse = std::max (channelsToUse.size(), (int) (editReader->getNumChannels()));

        for (int i = numChannelsToUse; --i >= 0;)
            channelState->emplace_back (0.0f);
    }

    // If we've just created a new reader, this will be the first
    // block with it in so we need to fade the block in
    isFirstBlock = true;

    return true;
}

void WaveNodeRealTime::replaceStateIfPossible (NodeGraph* nodeGraphToReplace)
{
    if (nodeGraphToReplace == nullptr)
        return;

    if (stateHash == 0)
        return;

    assert (getNodeProperties().nodeID == (size_t) editItemID.getRawID());

    if (auto oldWaveNode = findNodeWithID<WaveNodeRealTime> (*nodeGraphToReplace, (size_t) editItemID.getRawID()))
        replaceStateIfPossible (*oldWaveNode);
}

void WaveNodeRealTime::replaceStateIfPossible (WaveNodeRealTime& other)
{
    if (other.editItemID != editItemID)
        return;

    // This will be used to fade out the last block if the state hash is different
    channelState = other.channelState;

    if (other.stateHash != stateHash)
        return;

    fileTempoSequence = other.fileTempoSequence;
    fileTempoPosition = other.fileTempoPosition;
    resamplerReader = other.resamplerReader;
    editReader = other.editReader;
    pitchAdjustReader = other.pitchAdjustReader;
}

void WaveNodeRealTime::processSection (ProcessContext& pc)
{
    const auto sectionEditBeats = getEditBeatRange();
    const auto sectionEditTime = getEditTimeRange();

    // Check that the number of channels requested matches the destination buffer num channels
    assert (destChannels.size() == (int) pc.buffers.audio.getNumChannels());

    if (editReader == nullptr)
        return;

    if (editReader->isTimeBased()
        && (sectionEditTime.getEnd() <= editPositionTime.getStart()
            || sectionEditTime.getStart() >= editPositionTime.getEnd()))
        return;

    if (editReader->isBeatBased()
        && (sectionEditBeats.getEnd() <= editPositionBeats.getStart()
            || sectionEditBeats.getStart() >= editPositionBeats.getEnd()))
      return;


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

    if (pitchAdjustReader != nullptr)
        pitchAdjustReader->setKey (getKeyToSyncTo (sectionEditTime.getStart()));

    // Read through the audio stack
    const auto isContiguous = getPlayHeadState().isContiguousWithPreviousBlock();
    uint32_t lastSampleFadeLength = isFirstBlock ? std::min (numFrames, 40u) : 0;
    isFirstBlock = false;

    if (editReader->read (sectionEditBeats, sectionEditTime, pc.buffers.audio, isContiguous, getPlaybackSpeedRatio()))
    {
        if (! isContiguous && ! getPlayHeadState().isFirstBlockOfLoop())
            lastSampleFadeLength = std::min (numFrames, 40u);
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
}

tempo::Key WaveNodeRealTime::getKeyToSyncTo (TimePosition editPosition) const
{
    if (chordPitchPosition)
    {
        chordPitchPosition->set (editPosition);
        return chordPitchPosition->getKey();
    }

    return getKey();
}

}} // namespace tracktion { inline namespace engine
