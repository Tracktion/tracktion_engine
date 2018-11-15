/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class SubSampleWaveAudioNode    : public AudioNode
{
public:
    SubSampleWaveAudioNode (Engine& e,
                            const AudioFile& af,
                            EditTimeRange editTime,
                            double off,
                            EditTimeRange loop,
                            LiveClipLevel level,
                            double speed,
                            const juce::AudioChannelSet& channels)
        : engine (e),
          editPosition (editTime),
          loopSection (loop.getStart() * speed, loop.getEnd() * speed),
          offset (off),
          originalSpeedRatio (speed),
          audioFile (af),
          clipLevel (level),
          channelsToUse (channels)
    {
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        info.hasAudio = true;
        info.hasMidi = false;
        info.numberOfChannels =  juce::jlimit (1, channelsToUse.size(), audioFile.getNumChannels());
    }

    void visitNodes (const VisitorFn& v) override
    {
        v (*this);
    }

    bool purgeSubNodes (bool keepAudio, bool) override
    {
        return keepAudio;
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        reader = engine.getAudioFileManager().cache.createReader (audioFile);
        outputSampleRate = info.sampleRate;
        updateFileSampleRate();
        resetResamplers();
    }

    bool isReadyToRender() override
    {
        // if the hash is 0 it means an empty file path which means a missing file so
        // this will never return a valid reader and we should just bail
        if (audioFile.isNull())
            return true;

        if (reader == nullptr)
            if ((reader = engine.getAudioFileManager().cache.createReader (audioFile)) == nullptr)
                return false;

        if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
            return false;

        return true;
    }

    void releaseAudioNodeResources() override
    {
        reader = nullptr;
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        callRenderAdding (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        invokeSplitRender (rc, *this);
    }

    void prepareForNextBlock (const AudioRenderContext& rc) override
    {
        SCOPED_REALTIME_CHECK

        // keep a local copy, because releaseAudioNodeResources may remove the reader halfway through..
        if (const auto localReader = reader)
            localReader->setReadPosition ((juce::int64) (editTimeToFileSample (rc.getEditTime().editRange1.getStart()) + 0.5) - 5);
    }

    void renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
    {
        // keep a local copy, because releaseAudioNodeResources may remove the reader halfway through..
        const auto localReader (reader);

        rc.sanityCheck();

        if (rc.destBuffer == nullptr
             || rc.bufferNumSamples == 0
             || localReader == nullptr
             || editTime.getStart() >= editPosition.getEnd())
            return;

        SCOPED_REALTIME_CHECK

        if (audioFileSampleRate == 0.0 && ! updateFileSampleRate())
            return;

        const double fileStart = editTimeToFileSample (editTime.getStart());
        const double fileEnd   = editTimeToFileSample (editTime.getEnd());

        int preRead            = 5;
        auto fileReadStart     = juce::int64 (std::ceil (fileStart)) - preRead;
        auto fileReadEnd       = juce::int64 (std::ceil (fileEnd));
        auto numSamplesToRead  = (int) (fileReadEnd - fileReadStart);

        AudioScratchBuffer scratchBuffer (juce::jmin (2, rc.destBuffer->getNumChannels()), numSamplesToRead + 2);
        const juce::AudioChannelSet scratchBufferChannels = juce::AudioChannelSet::canonicalChannelSet (scratchBuffer.buffer.getNumChannels());

        localReader->setReadPosition (fileReadStart);

        int lastSampleFadeLength = 0;

        {
            SCOPED_REALTIME_CHECK

            if (localReader->readSamples (numSamplesToRead + 2, scratchBuffer.buffer, scratchBufferChannels, 0,
                                          channelsToUse,
                                          rc.isRendering ? 5000 : 3))
            {
                if (rc.isFirstBlockOfLoop() || ! rc.isContiguousWithPreviousBlock())
                    lastSampleFadeLength = rc.playhead.isUserDragging() ? 40 : 10;
            }
            else
            {
                lastSampleFadeLength = 40;
                scratchBuffer.buffer.clear();
            }
        }

        float gains[2];

        if (rc.destBuffer->getNumChannels() > 1)
            clipLevel.getLeftAndRightGains (gains[0], gains[1]);
        else
            gains[0] = gains[1] = clipLevel.getGainIncludingMute();

        if (rc.playhead.isUserDragging())
        {
            gains[0] *= 0.4f;
            gains[1] *= 0.4f;
        }

        // Pre-resampling set-up
        const double ratio = (fileEnd - fileStart) / (double) rc.bufferNumSamples;
        const double subSamplePos = fileStart - (int) fileStart;

        for (int channel = rc.destBuffer->getNumChannels(); --channel >= 0;)
        {
            const int srcChan = juce::jmin (channel, scratchBuffer.buffer.getNumChannels() - 1);
            const float* const src = scratchBuffer.buffer.getReadPointer (srcChan);
            float dest[5 + 2];
            const float gain = gains[channel & 1];

            // Stoke interpolator with pre-read samples and set next read position
            juce::LagrangeInterpolator& li = resampler[channel];
            li.reset();
            li.processAdding (1.0, src, dest, preRead - 1, gain);
            li.processAdding (subSamplePos, src + preRead - 1, dest, 1, gain);
        }

        if (ratio > 0.0)
        {
            for (int channel = rc.destBuffer->getNumChannels(); --channel >= 0;)
            {
                const int srcChan = juce::jmin (channel, scratchBuffer.buffer.getNumChannels() - 1);
                const float* const src = scratchBuffer.buffer.getReadPointer (srcChan, preRead);
                float* const dest = rc.destBuffer->getWritePointer (channel, rc.bufferStartSample);

                resampler[channel].processAdding (ratio, src, dest, rc.bufferNumSamples, gains[channel & 1]);

                if (lastSampleFadeLength > 0)
                {
                    const int fadeSamps = juce::jmin (lastSampleFadeLength, rc.bufferNumSamples);

                    for (int i = 0; i < fadeSamps; ++i)
                    {
                        const float alpha = i / (float) fadeSamps;
                        dest[i] = alpha * dest[i] + lastSample[channel] * (1.0f - alpha);
                    }
                }

                lastSample[channel] = dest[rc.bufferNumSamples - 1];
            }
        }
    }

    Engine& engine;

private:
    //==============================================================================
    EditTimeRange editPosition, loopSection;
    double offset;
    double originalSpeedRatio, outputSampleRate = 44100.0;

    AudioFile audioFile;
    LiveClipLevel clipLevel;
    double audioFileSampleRate = 0;
    juce::AudioChannelSet channelsToUse;
    AudioFileCache::Reader::Ptr reader;

    juce::LagrangeInterpolator resampler[8];
    float lastSample[8];

    double editTimeToFileSample (double editTime) const noexcept
    {
        return (editTime - (editPosition.getStart() - offset)) * originalSpeedRatio * audioFileSampleRate;
    }

    bool updateFileSampleRate()
    {
        if (reader != nullptr)
        {
            audioFileSampleRate = reader->getSampleRate();

            if (audioFileSampleRate > 0)
            {
                if (! loopSection.isEmpty())
                    reader->setLoopRange (juce::Range<juce::int64> ((juce::int64) (loopSection.getStart() * audioFileSampleRate),
                                                                    (juce::int64) (loopSection.getEnd()   * audioFileSampleRate)));

                return true;
            }
        }

        return false;
    }

    void resetResamplers()
    {
        for (int i = 0; i < juce::numElementsInArray (resampler); ++i)
            resampler[i].reset();

        for (int i = 0; i < juce::numElementsInArray (lastSample); ++i)
            lastSample[i] = 0.0f;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubSampleWaveAudioNode)
};

//==============================================================================
/** An AudioNode that speeds up and slows down its input node in/out at given times. */
class SpeedRampAudioNode : public SingleInputAudioNode
{
public:
    SpeedRampAudioNode (AudioNode* input,
                        EditTimeRange in,
                        EditTimeRange out,
                        AudioFadeCurve::Type fadeInType_,
                        AudioFadeCurve::Type fadeOutType_)
        : SingleInputAudioNode (input),
          fadeIn (in), fadeOut (out),
          fadeInType (fadeInType_),
          fadeOutType (fadeOutType_)
    {
        jassert (! (fadeIn.isEmpty() && fadeOut.isEmpty()));
    }

    //==============================================================================
    void renderOver (const AudioRenderContext& rc) override
    {
        if (renderingNeeded (rc))
            invokeSplitRender (rc, *this);
        else
            input->renderOver (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (renderingNeeded (rc))
            callRenderOver (rc);
        else
            input->renderAdding (rc);
    }

    void renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
    {
        const bool intersectsFadeIn  = fadeIn.getLength()  > 0.0 && editTime.overlaps (fadeIn);
        const bool intersectsFadeOut = fadeOut.getLength() > 0.0 && editTime.overlaps (fadeOut);

        if (intersectsFadeIn && intersectsFadeOut)
        {
            auto startSample = rc.bufferStartSample;
            auto numSamples = rc.bufferNumSamples;
            auto sampleRate = numSamples / editTime.getLength();
            auto streamTime = rc.streamTime;

            {
                EditTimeRange fadeInSection (editTime.getStart(), fadeOut.getStart());
                auto fadeInTime = fadeInSection.getLength();

                AudioRenderContext rc2 (rc);
                rc2.streamTime = rc.streamTime.withLength (fadeInTime);
                rc2.bufferNumSamples = juce::roundToInt (fadeInTime * sampleRate);
                renderRampSection (rc2, fadeInSection, fadeIn, true);

                numSamples -= rc2.bufferNumSamples;
                startSample += rc2.bufferNumSamples;
                streamTime.start = rc2.streamTime.getEnd();
            }

            {
                AudioRenderContext rc2 (rc);
                rc2.streamTime = streamTime;
                rc2.bufferNumSamples = numSamples;
                rc2.bufferStartSample = startSample;
                startSample += rc2.bufferNumSamples;

                renderRampSection (rc2, fadeOut.getIntersectionWith (editTime), fadeOut, false);
            }

            juce::ignoreUnused (startSample);
            jassert (startSample == (rc.bufferNumSamples + rc.bufferStartSample));
        }
        else if (intersectsFadeIn)
        {
            renderRampSection (rc, editTime, fadeIn, true);
        }
        else if (intersectsFadeOut)
        {
            renderRampSection (rc, editTime, fadeOut, false);
        }
    }

private:
    //==============================================================================
    EditTimeRange fadeIn, fadeOut;
    AudioFadeCurve::Type fadeInType, fadeOutType;

    bool renderingNeeded (const AudioRenderContext& rc) const
    {
        if (rc.destBuffer == nullptr || ! rc.playhead.isPlaying())
            return false;

        auto editTime = rc.getEditTime();

        if (editTime.isSplit)
            return fadeIn.overlaps (editTime.editRange1)
                    || fadeIn.overlaps (editTime.editRange2)
                    || fadeOut.overlaps (editTime.editRange1)
                    || fadeOut.overlaps (editTime.editRange2);

        return fadeIn.overlaps (editTime.editRange1)
                || fadeOut.overlaps (editTime.editRange1);
    }

    void renderRampSection (const AudioRenderContext& rc, EditTimeRange editTime,
                            EditTimeRange fade, bool rampUp)
    {
        auto startSample = rc.bufferStartSample;
        auto sampleRate = rc.bufferNumSamples / editTime.getLength();
        auto timeBefore = fade.getStart() - editTime.getStart();

        if (timeBefore > 0.0)
        {
            auto numSamples = juce::roundToInt (timeBefore * sampleRate);

            if (numSamples > 0)
            {
                AudioRenderContext rc2 (rc);
                rc2.streamTime = rc.streamTime.withLength (timeBefore);
                rc2.bufferNumSamples = numSamples;
                startSample += numSamples;

                input->renderOver (rc2);
            }
        }

        auto editTimeIntersection = fade.getIntersectionWith (editTime);

        if (editTimeIntersection.getLength() > 0.0)
        {
            auto startAlpha = (editTimeIntersection.getStart() - fade.getStart()) / fade.getLength();
            auto endAlpha   = (editTimeIntersection.getEnd()   - fade.getStart()) / fade.getLength();

            const AudioFadeCurve::Type t = rampUp ? fadeInType : fadeOutType;
            auto startProp = rescale (t, startAlpha, rampUp);
            auto endProp   = rescale (t, endAlpha,   rampUp);

            jassert (juce::isPositiveAndNotGreaterThan (startProp, 1.0)
                      && juce::isPositiveAndNotGreaterThan (endProp, 1.0));

            EditTimeRange newEditTime (fade.getStart() + fade.getLength() * startProp,
                                       fade.getStart() + fade.getLength() * endProp);

            auto numSamples = juce::roundToInt (editTimeIntersection.getLength() * sampleRate);
            auto streamDiff = rc.streamTime.getStart() - editTime.getStart();
            AudioRenderContext rc2 (rc);
            rc2.streamTime = newEditTime + streamDiff;
            rc2.bufferNumSamples = numSamples;
            rc2.bufferStartSample = startSample;
            startSample += numSamples;

            input->renderOver (rc2);
        }

        auto timeAfter = editTime.getEnd() - fade.getEnd();

        if (timeAfter > 0.0)
        {
            auto numSamples = juce::roundToInt (timeAfter * sampleRate);

            if (numSamples > 0)
            {
                AudioRenderContext rc2 (rc);
                rc2.streamTime = { rc.streamTime.getEnd() - timeAfter, rc.streamTime.getEnd() };
                rc2.bufferNumSamples = numSamples;
                rc2.bufferStartSample = startSample;
                startSample += numSamples;

                input->renderOver (rc2);
            }
        }

        juce::ignoreUnused (startSample);
        jassert (startSample == (rc.bufferNumSamples + rc.bufferStartSample));
    }

    static double rescale (AudioFadeCurve::Type t, double proportion, bool rampUp)
    {
        switch (t)
        {
            case AudioFadeCurve::convex:
                return rampUp ? (-2.0 * std::cos ((juce::double_Pi * proportion) / 2.0)) / juce::double_Pi + 1.0
                              : 1.0 - ((-2.0 * std::cos ((juce::double_Pi * (proportion - 1.0)) / 2.0)) / juce::double_Pi + 1.0);

            case AudioFadeCurve::concave:
                return rampUp ? proportion - (2.0 * std::sin ((juce::double_Pi * proportion) / 2.0)) / juce::double_Pi + (2.0 / juce::double_Pi)
                              : ((2.0 * std::sin ((juce::double_Pi * (proportion + 1.0)) / 2.0)) / juce::double_Pi) + proportion - (2.0 / juce::double_Pi);

            case AudioFadeCurve::sCurve:
                return rampUp ? (proportion / 2.0) - (std::sin (juce::double_Pi * proportion) / (2.0 * juce::double_Pi)) + 0.5
                              : std::sin (juce::double_Pi * proportion) / (2.0 * juce::double_Pi) + (proportion / 2.0);

            default:
                return rampUp ? (juce::square (proportion) * 0.5) + 0.5
                              : ((-juce::square (proportion - 1.0)) * 0.5) + 0.5;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpeedRampAudioNode)
};

} // namespace tracktion_engine
