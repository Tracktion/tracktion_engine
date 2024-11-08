/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{


class SampleFader
{
public:
    SampleFader() = default;

    SampleFader (size_t numChannels)
    {
        reset (numChannels);
    }

    void reset (size_t numChannels)
    {
        samples = std::vector<float> (numChannels, 0.0f);
    }

    size_t getNumChannels() const
    {
        return samples.size();
    }

    template<typename Buffer>
    void push (const Buffer& buffer)
    {
        pushSingleFrame (buffer.getEnd (1));
    }

    void trigger (size_t numFramesToFade_)
    {
        numFramesToFade = numFramesToFade_;
        currentFadeFrameCountDown = numFramesToFade;
    }

    enum class FadeType
    {
        fadeOut,
        crossfade
    };

    template<typename Buffer>
    void apply (Buffer&& buffer, FadeType fadeType)
    {
        if (currentFadeFrameCountDown == 0 || numFramesToFade == 0)
            return;

        const auto numFrames = static_cast<size_t> (buffer.getNumFrames());
        const auto numChannels = buffer.getNumChannels();
        const size_t numThisTime = std::min (numFrames, currentFadeFrameCountDown);

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            const auto dest = buffer.getIterator (channel).sample;
            const auto lastSample = samples[static_cast<size_t> (channel)];
            auto channelCurrentFadeFrameCountDown = currentFadeFrameCountDown;

            for (size_t i = 0; i < numThisTime; ++i)
            {
                const auto frameNum = numFramesToFade - channelCurrentFadeFrameCountDown;
                auto alpha = frameNum / static_cast<float> (numFramesToFade);
                assert (alpha >= 0.0f && alpha <= 1.0f);

                if (fadeType == FadeType::fadeOut)
                    dest[i] = dest[i] + (lastSample * (1.0f - alpha));
                else if (fadeType == FadeType::crossfade)
                    dest[i] = (alpha * dest[i]) + (lastSample * (1.0f - alpha));

                --channelCurrentFadeFrameCountDown;
            }
        }

        assert ((static_cast<int> (currentFadeFrameCountDown) - static_cast<int> (numThisTime)) >= 0);
        currentFadeFrameCountDown -= numThisTime;
    }

    template<typename Buffer>
    void applyAt (Buffer& buffer, choc::buffer::FrameCount frameNum, FadeType fadeType)
    {
        apply (buffer.getFrameRange (choc::buffer::FrameRange { .start = frameNum, .end = buffer.getNumFrames() - frameNum }),
               fadeType);
    }

private:
    std::vector<float> samples;
    size_t numFramesToFade = 0, currentFadeFrameCountDown = 0;

    template<typename Buffer>
    void pushSingleFrame (const Buffer& buffer)
    {
        const auto numChannels = buffer.getNumChannels();
        assert (numChannels == samples.size());
        assert (buffer.getNumFrames() == 1u);

        for (auto chan = 0u; chan < numChannels; ++chan)
            samples[static_cast<size_t> (chan)] = buffer.getSample (chan, 0);
    }
};

class SlotControlNode;

//==============================================================================
//==============================================================================
/**
    A Node that switches between two of its inputs based on a flag.
*/
class ArrangerLauncherSwitchingNode final    : public tracktion::graph::Node,
                                               public TracktionEngineNode,
                                               public SharedTimer::Listener
{
public:
    ArrangerLauncherSwitchingNode (ProcessState&,
                                   AudioTrack&,
                                   std::unique_ptr<Node> arrangerNode,
                                   std::vector<std::unique_ptr<SlotControlNode>> launcherNodes);

    std::vector<Node*> getDirectInputNodes() override;
    std::vector<Node*> getInternalNodes() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const PlaybackInitialisationInfo&) override;

    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    struct SlotClipStatus;

    AudioTrack::Ptr track;
    std::unique_ptr<Node> arrangerNode;
    std::vector<std::unique_ptr<SlotControlNode>> launcherNodes;
    std::vector<SlotControlNode*> launcherNodesCopy;
    std::shared_ptr<SampleFader> launcherSampleFader, arrangerSampleFader;
    std::shared_ptr<ActiveNoteList> arrangerActiveNoteList;
    std::shared_ptr<std::atomic<ArrangerLauncherSwitchingNode*>> activeNode;
    MPESourceID midiSourceID = createUniqueMPESourceID();
    ScopedListener playSlotsUpdaterListener { track->edit.engine.getBackToArrangerUpdateTimer(),  *this };

    //==============================================================================
    void processLauncher (ProcessContext&, const SlotClipStatus&);
    void processArranger (ProcessContext&, const SlotClipStatus&);

    void sortPlayingOrQueuedClipsFirst();
    void updatePlaySlotsState();

    //==============================================================================
    static choc::buffer::FrameCount beatToSamplePosition (std::optional<BeatDuration> beat, BeatDuration numBeats, choc::buffer::FrameCount);

    struct SlotClipStatus
    {
        bool anyClipsPlaying = false;
        bool anyClipsQueued = false;
        std::optional<BeatDuration> beatsUntilQueuedStart;
        std::optional<BeatDuration> beatsUntilQueuedStartTrimmedToBlock;
        std::optional<BeatDuration> beatsUntilQueuedStopTrimmedToBlock;
    };

    static SlotClipStatus getSlotsStatus (const std::vector<std::unique_ptr<SlotControlNode>>&,
                                         BeatRange editBeatRange, MonotonicBeat);

    //==============================================================================
    void sharedTimerCallback() override;
};

}} // namespace tracktion { inline namespace engine
