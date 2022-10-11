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
/** An Node that plays back a wave file. */
class WaveNode final    : public tracktion_graph::Node,
                          public TracktionEngineNode
{
public:
    /** offset is a time added to the start of the file, e.g. an offset of 10.0
        would produce ten seconds of silence followed by the file.

        gain may be 0, or a pointer to a floating point value which is referred to as the gain
        to use when converting the file contents to floating point. e.g. gain of
        2.0f will double the values returned.

    */
    WaveNode (const AudioFile&,
              EditTimeRange editTime,
              double offset,
              EditTimeRange loopSection,
              LiveClipLevel,
              double speedRatio,
              const juce::AudioChannelSet& sourceChannelsToUse,
              const juce::AudioChannelSet& destChannelsToFill,
              ProcessState&,
              EditItemID,
              bool isOfflineRender);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    EditTimeRange editPosition, loopSection;
    double offset = 0;
    double originalSpeedRatio = 0, outputSampleRate = 44100.0;
    const EditItemID editItemID;
    bool isOfflineRender = false;

    AudioFile audioFile;
    LiveClipLevel clipLevel;
    juce::Range<int64_t> editPositionInSamples;
    double audioFileSampleRate = 0;
    const juce::AudioChannelSet channelsToUse, destChannels;
    AudioFileCache::Reader::Ptr reader;

    struct PerChannelState;
    std::shared_ptr<juce::OwnedArray<PerChannelState>> channelState;

    int64_t editPositionToFileSample (int64_t) const noexcept;
    int64_t editTimeToFileSample (double) const noexcept;
    bool updateFileSampleRate();
    void replaceChannelStateIfPossible (Node*, int numChannelsToUse);
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveNode)
};

} // namespace tracktion_engine
