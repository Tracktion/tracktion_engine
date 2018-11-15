/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class TimedMutingAudioNode  : public SingleInputAudioNode
{
public:
    TimedMutingAudioNode (AudioNode* inp, const juce::Array<EditTimeRange>& muteTimes_)
        : SingleInputAudioNode (inp), muteTimes (muteTimes_)
    {
    }

    void renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
    {
        for (auto r : muteTimes)
        {
            if (r.overlaps (editTime))
            {
                auto mute = r.getIntersectionWith (editTime);

                if (! mute.isEmpty())
                {
                    if (mute == editTime)
                    {
                        muteBuffer (rc, 0, rc.bufferNumSamples);
                    }
                    else if (editTime.contains (mute))
                    {
                        auto startSample = timeToSample (rc, mute.getStart() - editTime.getStart());
                        auto numSamples = timeToSample (rc, mute.getEnd() - mute.getStart());
                        muteBuffer (rc, startSample, numSamples);
                    }
                    else if (mute.getEnd() <= editTime.getEnd())
                    {
                        muteBuffer (rc, 0, timeToSample (rc, editTime.getEnd() - mute.getEnd()));
                    }
                    else if (mute.getStart() >= editTime.getStart())
                    {
                        auto startSample = timeToSample (rc, mute.getStart() - editTime.getStart());
                        muteBuffer (rc, startSample, rc.bufferNumSamples - startSample);
                    }
                }
            }

            if (r.getEnd() >= editTime.getEnd())
                return;
        }
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        input->renderOver (rc);

        if (renderingNeeded (rc))
            invokeSplitRender (rc, *this);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (renderingNeeded (rc))
            callRenderOver (rc);
        else
            input->renderAdding (rc);
    }

private:
    juce::Array<EditTimeRange> muteTimes;

    bool renderingNeeded (const AudioRenderContext& rc) const
    {
        if (rc.destBuffer == nullptr || ! rc.playhead.isPlaying())
            return false;

        return true;
    }

    void muteBuffer (const AudioRenderContext& rc, int startSample, int numSamples)
    {
        for (int i = rc.destBuffer->getNumChannels(); --i >= 0;)
            juce::FloatVectorOperations::clear (rc.destBuffer->getWritePointer (i, rc.bufferStartSample + startSample), numSamples);
    }

    static int timeToSample (const AudioRenderContext& rc, double t)
    {
        auto length = rc.streamTime.getLength();

        if (length > 0.0)
            return (int) (rc.bufferNumSamples * (t / length) + 0.5);

        return 0;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimedMutingAudioNode)
};

} // namespace tracktion_engine
