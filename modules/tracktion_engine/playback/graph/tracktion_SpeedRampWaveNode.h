/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
/** Describes the time and type of the speed fade in/outs.*/
struct SpeedFadeDescription
{
    TimeRange inTimeRange, outTimeRange;
    AudioFadeCurve::Type fadeInType, fadeOutType;

    bool isEmpty() const
    {
        return inTimeRange.isEmpty() && outTimeRange.isEmpty();
    }
};


//==============================================================================
//==============================================================================
/** An Node that plays back a wave file. */
class SpeedRampWaveNode final   : public tracktion::graph::Node,
                                  public TracktionEngineNode
{
public:
    /** offset is a time added to the start of the file, e.g. an offset of 10.0
        would produce ten seconds of silence followed by the file.

        gain may be 0, or a pointer to a floating point value which is referred to as the gain
        to use when converting the file contents to floating point. e.g. gain of
        2.0f will double the values returned.

    */
    SpeedRampWaveNode (const AudioFile&,
                       TimeRange editTime,
                       TimeDuration offset,
                       TimeRange loopSection,
                       LiveClipLevel,
                       double speedRatio,
                       const juce::AudioChannelSet& sourceChannelsToUse,
                       const juce::AudioChannelSet& destChannelsToFill,
                       ProcessState&,
                       EditItemID,
                       bool isOfflineRender,
                       SpeedFadeDescription);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    TimeRange editPosition, loopSection;
    TimeDuration offset;
    double originalSpeedRatio = 0, outputSampleRate = 44100.0;
    const EditItemID editItemID;
    bool isOfflineRender = false;
    const SpeedFadeDescription speedFadeDescription;

    AudioFile audioFile;
    LiveClipLevel clipLevel;
    juce::Range<int64_t> editPositionInSamples;
    double audioFileSampleRate = 0;
    const juce::AudioChannelSet channelsToUse, destChannels;
    AudioFileCache::Reader::Ptr reader;

    struct PerChannelState;
    juce::OwnedArray<PerChannelState> channelState;
    bool playedLastBlock = false;

    //==============================================================================
    int64_t editTimeToFileSample (TimePosition) const noexcept;
    bool updateFileSampleRate();
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);

    //==============================================================================
    static double rescale (AudioFadeCurve::Type, double proportion, bool rampUp);
};

}} // namespace tracktion { inline namespace engine

#ifndef DOXYGEN
template<>
struct std::hash<tracktion::engine::SpeedFadeDescription>
{
    size_t operator() (const tracktion::engine::SpeedFadeDescription& d) const noexcept
    {
        size_t seed = 0;
        tracktion::hash_combine (seed, d.inTimeRange);
        tracktion::hash_combine (seed, d.outTimeRange);
        tracktion::hash_combine (seed, static_cast<int> (d.fadeInType));
        tracktion::hash_combine (seed, static_cast<int> (d.fadeOutType));

        return seed;
    }
};
#endif
