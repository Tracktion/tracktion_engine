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
/** This class is a necessary bodge due to ARA needing to be told that we're playing, even if we aren't,
    so it can generate audio while its graph is being modified!
*/
class MelodyneNode::MelodynePlayhead  : public juce::AudioPlayHead
{
public:
    MelodynePlayhead (ExternalPlugin& p)
        : plugin (p)
    {
    }

    /** Must be called before processing audio/MIDI */
    void setCurrentInfo (TimePosition currentTimeSeconds, bool playing,
                         bool looping, TimeRange loopTimes)
    {
        time = currentTimeSeconds;
        isPlaying = playing;
        isLooping = looping;
        loopTimeRange = loopTimes;

        loopStart.set (loopTimeRange.getStart());
        loopEnd.set (loopTimeRange.getEnd());
        currentPos.set (time);
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo result;

        result.setFrameRate (getFrameRate());

        auto& transport = plugin.edit.getTransport();

        result.setIsPlaying (isPlaying);
        result.setIsLooping (isLooping);
        result.setIsRecording (transport.isRecording());
        result.setEditOriginTime (transport.getTimeWhenStarted().inSeconds());

        if (isLooping)
            result.setLoopPoints (juce::AudioPlayHead::LoopPoints ({ loopStart.getPPQTime(), loopEnd.getPPQTime() }));

        result.setTimeInSeconds (time.inSeconds());
        result.setTimeInSamples (toSamples (time, plugin.getAudioPluginInstance()->getSampleRate()));

        const auto timeSig = currentPos.getTimeSignature();
        result.setBpm (currentPos.getTempo());
        result.setTimeSignature (juce::AudioPlayHead::TimeSignature ({ timeSig.numerator, timeSig.denominator }));

        const auto ppqPositionOfLastBarStart = currentPos.getPPQTimeOfBarStart();
        result.setPpqPositionOfLastBarStart (ppqPositionOfLastBarStart);
        result.setPpqPosition (std::max (ppqPositionOfLastBarStart, currentPos.getPPQTime()));

        return result;
    }

private:
    ExternalPlugin& plugin;
    tempo::Sequence::Position currentPos { createPosition (plugin.edit.tempoSequence) };
    tempo::Sequence::Position loopStart { createPosition (plugin.edit.tempoSequence) };
    tempo::Sequence::Position loopEnd { createPosition (plugin.edit.tempoSequence) };
    TimePosition time;
    bool isPlaying = false, isLooping = false;
    TimeRange loopTimeRange;

    AudioPlayHead::FrameRateType getFrameRate() const
    {
        switch (plugin.edit.getTimecodeFormat().getFPS())
        {
            case 24:    return AudioPlayHead::fps24;
            case 25:    return AudioPlayHead::fps25;
            case 29:    return AudioPlayHead::fps30drop;
            case 30:    return AudioPlayHead::fps30;
            default:    break;
        }

        return AudioPlayHead::fps30; //Just to cope with it.
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodynePlayhead)
};

//==============================================================================
//==============================================================================
MelodyneNode::MelodyneNode (AudioClipBase& c, tracktion::graph::PlayHead& ph, bool isRendering)
    : clip (c), playHead (ph),
      clipLevel (clip.getLiveClipLevel()), clipPtr (&c),
      melodyneProxy (c.melodyneProxy),
      fileInfo (clip.getAudioFile().getInfo()),
      isOfflineRender (isRendering)
{
    // Only need to check the analysing state when we're rendering
    if (isOfflineRender)
    {
        updateAnalysingState();

        if (analysingContent)
            startTimerHz (10);
    }
}

MelodyneNode::~MelodyneNode()
{
    CRASH_TRACER

    if (auto ep = melodyneProxy->getPlugin())
        if (auto p = ep->getAudioPluginInstance())
            p->setPlayHead (nullptr);

    playhead.reset();
    melodyneProxy = nullptr;
}

//==============================================================================
tracktion::graph::NodeProperties MelodyneNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = fileInfo.numChannels;
    
    if (auto plugin = melodyneProxy->getPlugin())
        if (auto p = plugin->getAudioPluginInstance())
            props.numberOfChannels = juce::jmax (props.numberOfChannels, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    
    return props;
}

std::vector<tracktion::graph::Node*> MelodyneNode::getDirectInputNodes()
{
    return {};
}

void MelodyneNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    CRASH_TRACER
    if (auto plugin = melodyneProxy->getPlugin())
    {
        if (auto p = plugin->getAudioPluginInstance())
        {
            //NB: Reducing the number of calls to juce::AudioProcessor::prepareToPlay() keeps Celemony happy;
            //    the VST3 version of their plugin relies heavily on calls to the inconspicuous IComponent::setActive()!
            if (p->getSampleRate() != info.sampleRate
                 || p->getBlockSize() != info.blockSize)
            {
                plugin->initialise ({ TimePosition(), info.sampleRate, info.blockSize });
            }

            p->setPlayHead (nullptr);
            playhead = std::make_unique<MelodynePlayhead> (*plugin);
            p->setPlayHead (playhead.get());

            desc = p->getPluginDescription();
        }
    }
}

bool MelodyneNode::isReadyToProcess()
{
    if (! isOfflineRender)
        return true;
    
    return ! analysingContent;
}

void MelodyneNode::process (ProcessContext& pc)
{
    CRASH_TRACER
    auto& dest = pc.buffers.audio;

    if (dest.getNumFrames() == 0 || dest.getNumChannels() == 0 || melodyneProxy == nullptr)
        return;

    if (auto plugin = melodyneProxy->getPlugin())
    {
        if (auto pluginInstance = plugin->getAudioPluginInstance())
        {
            if (pluginInstance->getPlayHead() != playhead.get())
                pluginInstance->setPlayHead (playhead.get()); //This is needed in case the ExternalPlugin back-end has swapped the playhead out

            midiMessages.clear();

            // Update PlayHead
            {
                const auto timelinePosition = referenceSampleRangeToSplitTimelineRange (playHead, pc.referenceSampleRange).timelineRange1.getStart();
                const auto loopPositions = playHead.getLoopRange();
                const auto sampleRate = pluginInstance->getSampleRate();
                playhead->setCurrentInfo (TimePosition::fromSamples (timelinePosition, sampleRate),
                                          playHead.isPlaying(), playHead.isLooping(),
                                          tracktion::timeRangeFromSamples (loopPositions, sampleRate));
            }

            auto asb = tracktion::graph::toAudioBuffer (dest);
            pluginInstance->processBlock (asb, midiMessages);

            float gains[2];

            if (asb.getNumChannels() > 1)
                clipLevel.getLeftAndRightGains (gains[0], gains[1]);
            else
                gains[0] = gains[1] = clipLevel.getGainIncludingMute();

            if (playHead.isUserDragging())
            {
                gains[0] *= 0.4f;
                gains[1] *= 0.4f;
            }

            for (int i = 0; i < asb.getNumChannels(); ++i)
            {
                const float gain = gains[i & 1];

                if (gain != 1.0f)
                    asb.applyGain (i, 0, asb.getNumSamples(), gain);
            }
        }
    }
}

//==============================================================================
void MelodyneNode::updateAnalysingState()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    analysingContent = melodyneProxy->isAnalysingContent();

    if (! analysingContent)
        stopTimer();
}

void MelodyneNode::timerCallback()
{
    updateAnalysingState();
}

}} // namespace tracktion { inline namespace engine
