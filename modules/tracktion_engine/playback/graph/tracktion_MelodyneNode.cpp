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
/** This class is a necessary bodge due to ARA needing to be told that we're playing, even if we aren't,
    so it can generate audio while its graph is being modified!
*/
class MelodyneNode::MelodynePlayhead  : public juce::AudioPlayHead
{
public:
    MelodynePlayhead (ExternalPlugin& p)
        : plugin (p)
    {
        currentPos = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
        loopStart  = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
        loopEnd    = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
    }

    /** Must be called before processing audio/MIDI */
    void setCurrentInfo (double currentTimeSeconds, bool playing,
                         bool looping, EditTimeRange loopTimes)
    {
        time = currentTimeSeconds;
        isPlaying = playing;
        looping = isLooping;
        loopTimeRange = loopTimes;
    }

    bool getCurrentPosition (CurrentPositionInfo& result) override
    {
        zerostruct (result);
        result.frameRate = getFrameRate();

        auto& transport = plugin.edit.getTransport();

        result.isPlaying        = isPlaying;
        result.isLooping        = isLooping;
        result.isRecording      = transport.isRecording();
        result.editOriginTime   = transport.getTimeWhenStarted();

        if (result.isLooping)
        {
            loopStart->setTime (loopTimeRange.start);
            result.ppqLoopStart = loopStart->getPPQTime();

            loopEnd->setTime (loopTimeRange.end);
            result.ppqLoopEnd   = loopEnd->getPPQTime();
        }

        result.timeInSeconds    = time;
        result.timeInSamples    = (int64_t) (time * plugin.getAudioPluginInstance()->getSampleRate());

        currentPos->setTime (time);
        const auto& tempo = currentPos->getCurrentTempo();
        result.bpm                  = tempo.bpm;
        result.timeSigNumerator     = tempo.numerator;
        result.timeSigDenominator   = tempo.denominator;

        result.ppqPositionOfLastBarStart = currentPos->getPPQTimeOfBarStart();
        result.ppqPosition = std::max (result.ppqPositionOfLastBarStart, currentPos->getPPQTime());

        return true;
    }

private:
    ExternalPlugin& plugin;
    std::unique_ptr<TempoSequencePosition> currentPos, loopStart, loopEnd;
    double time = 0;
    bool isPlaying = false, isLooping = false;
    EditTimeRange loopTimeRange;

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
MelodyneNode::MelodyneNode (AudioClipBase& c, tracktion_graph::PlayHead& ph, bool isRendering)
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
tracktion_graph::NodeProperties MelodyneNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = fileInfo.numChannels;
    
    if (auto plugin = melodyneProxy->getPlugin())
        if (auto p = plugin->getAudioPluginInstance())
            props.numberOfChannels = juce::jmax (props.numberOfChannels, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    
    return props;
}

std::vector<tracktion_graph::Node*> MelodyneNode::getDirectInputNodes()
{
    return {};
}

void MelodyneNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
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
                tracktion_engine::PlayHead enginePlayHead;
                tracktion_engine::PlaybackInitialisationInfo teInfo =
                {
                    0.0, info.sampleRate, info.blockSize, {}, enginePlayHead
                };

                plugin->initialise (teInfo);
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
                playhead->setCurrentInfo (tracktion_graph::sampleToTime (timelinePosition, sampleRate),
                                          playHead.isPlaying(), playHead.isLooping(),
                                          tracktion_graph::sampleToTime (loopPositions, sampleRate));
            }

            auto asb = tracktion_graph::toAudioBuffer (dest);
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

} // namespace tracktion_engine
