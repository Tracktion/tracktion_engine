/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_SpeedRampWaveNode.h"

namespace tracktion { inline namespace engine
{

class TimeRangeRemappingReader;
class EditToClipBeatReader;
class EditToClipTimeReader;
class ResamplerReader;
class PitchAdjustReader;
class SpeedFadeEditReader;

struct WarpPoint
{
    TimePosition sourceTime, warpTime;
};

using WarpMap = std::vector<WarpPoint>;

//==============================================================================
//==============================================================================
/** An Node that plays back a wave file. */
class WaveNode final    : public tracktion::graph::Node,
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
              TimeRange editTime,
              TimeDuration offset,
              TimeRange loopSection,
              LiveClipLevel,
              double speedRatio,
              const juce::AudioChannelSet& sourceChannelsToUse,
              const juce::AudioChannelSet& destChannelsToFill,
              ProcessState&,
              EditItemID,
              bool isOfflineRender);

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

    AudioFile audioFile;
    LiveClipLevel clipLevel;
    juce::Range<int64_t> editPositionInSamples;
    double audioFileSampleRate = 0;
    const juce::AudioChannelSet channelsToUse, destChannels;
    AudioFileCache::Reader::Ptr reader;

    struct PerChannelState;
    std::shared_ptr<juce::OwnedArray<PerChannelState>> channelState;

    int64_t editPositionToFileSample (int64_t) const noexcept;
    int64_t editTimeToFileSample (TimePosition) const noexcept;
    bool updateFileSampleRate();
    void replaceChannelStateIfPossible (NodeGraph*, int numChannelsToUse);
    void replaceChannelStateIfPossible (WaveNode&, int numChannelsToUse);
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveNode)
};

//==============================================================================
//==============================================================================
/**
    An Node that plays back a wave file.
*/
class WaveNodeRealTime final    : public graph::Node,
                                  public TracktionEngineNode,
                                  public DynamicallyOffsettableNodeBase
{
public:
    //==============================================================================
    /** Whether or not to use a background thread to read ahead the time-stretch buffer. */
    enum class ReadAhead : bool { no, yes };

    //==============================================================================
    /** offset is a time added to the start of the file, e.g. an offset of 10.0
        would produce ten seconds of silence followed by the file.
    */
    WaveNodeRealTime (const AudioFile&,
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
                      ResamplingQuality = ResamplingQuality::lagrange,
                      SpeedFadeDescription = {},
                      std::optional<tempo::Sequence::Position> editTempoSequence = {},
                      TimeStretcher::Mode = TimeStretcher::Mode::disabled,
                      TimeStretcher::ElastiqueProOptions = {},
                      float pitchChangeSemitones = 0.0f,
                      ReadAhead = ReadAhead::no);

    //==============================================================================
    /** Represets whether the file should try and match Edit tempo changes. */
    enum class SyncTempo : bool { no, yes };

    /** Represets whether the file should try and match Edit pitch changes. */
    enum class SyncPitch : bool { no, yes };

    /** Options for a beat-based WaveNodeRealTime. */
    struct BeatConfig
    {
        ProcessState& processState;
        const AudioFile& audioFile;
        TimeStretcher::Mode timeStretchMode = TimeStretcher::Mode::disabled;
        TimeStretcher::ElastiqueProOptions elastiqueProOptions;
        BeatRange editTime;
        BeatDuration offset;
        BeatRange loopSection;
        LiveClipLevel liveClipLevel;
        juce::AudioChannelSet sourceChannelsToUse;
        juce::AudioChannelSet destChannelsToFill;
        EditItemID itemID;
        bool isOfflineRender = false;
        ResamplingQuality resamplingQuality = ResamplingQuality::lagrange;
        SpeedFadeDescription speedFadeDescription;
        std::optional<tempo::Sequence::Position> editTempoSequence;
        std::optional<WarpMap> warpMap;
        tempo::Sequence sourceFileTempoMap;
        SyncTempo syncTempo = SyncTempo::no;
        SyncPitch syncPitch = SyncPitch::no;
        std::optional<tempo::Sequence> chordPitchSequence;
        float pitchChangeSemitones = 1.0f;
        ReadAhead readAhead = ReadAhead::no;
    };

    /** Constructs a beat-based WaveNodeRealTime. */
    WaveNodeRealTime (BeatConfig);

    /**
        @param sourceFileTempoMap   A tempo map describing the changes in the source file.
                                    This is relative to the start of the file, not the
                                    positioning in the Edit.
                                    This map is also used to describe a single tempo/time sig
                                    for the file with a pair of changes at position 0. With this,
                                    the file playback will match tempo changes in the Edit.
        @param chordPitchSequence   If this is supplied and SyncPitch == yes, rather than syncing
                                    to the Edit's pitch sequence, it will sync to this pitch sequence.
        @param pitchChangeSemitones Is SyncPitch == no, then this can be used to change the pitch
                                    of the source
    */
    WaveNodeRealTime (const AudioFile&,
                      TimeStretcher::Mode,
                      TimeStretcher::ElastiqueProOptions,
                      BeatRange editTime,
                      BeatDuration offset,
                      BeatRange loopSection,
                      LiveClipLevel,
                      const juce::AudioChannelSet& sourceChannelsToUse,
                      const juce::AudioChannelSet& destChannelsToFill,
                      ProcessState&,
                      EditItemID,
                      bool isOfflineRender,
                      ResamplingQuality,
                      SpeedFadeDescription,
                      std::optional<tempo::Sequence::Position> editTempoSequence,
                      std::optional<WarpMap>,
                      tempo::Sequence sourceFileTempoMap,
                      SyncTempo, SyncPitch,
                      std::optional<tempo::Sequence> chordPitchSequence,
                      float pitchChangeSemitones = 1.0f,
                      ReadAhead = ReadAhead::no);

    //==============================================================================
    /** Sets an offset to be applied to all times in this node, effectively shifting
        it forwards or backwards in time.
    */
    void setDynamicOffsetBeats (BeatDuration) override;

    //==============================================================================
    graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    BeatRange editPositionBeats, loopSectionBeats;
    BeatDuration offsetBeats;
    TimeRange editPositionTime, loopSectionTime;
    TimeDuration offsetTime;
    const double speedRatio = 1.0;
    const EditItemID editItemID;
    const bool isOfflineRender = false;
    const ResamplingQuality resamplingQuality;

    const AudioFile audioFile;
    const SpeedFadeDescription speedFadeDescription;
    const std::optional<tempo::Sequence::Position> editTempoSequence;
    std::optional<WarpMap> warpMap;
    TimeStretcher::Mode timeStretcherMode;
    TimeStretcher::ElastiqueProOptions elastiqueProOptions;
    LiveClipLevel clipLevel;
    const juce::AudioChannelSet channelsToUse, destChannels;
    float pitchChangeSemitones = 0.0;
    double outputSampleRate = 44100.0;
    int outputBlockSize = 0;
    bool isFirstBlock = false;
    const ReadAhead readAhead;

    size_t stateHash = 0;
    ResamplerReader* resamplerReader = nullptr;
    PitchAdjustReader* pitchAdjustReader = nullptr;
    std::shared_ptr<SpeedFadeEditReader> editReader;
    std::shared_ptr<std::vector<float>> channelState;
    std::shared_ptr<BeatDuration> dynamicOffsetBeats = std::make_shared<BeatDuration>();

    std::shared_ptr<tempo::Sequence> fileTempoSequence;
    std::shared_ptr<tempo::Sequence::Position> fileTempoPosition;
    SyncTempo syncTempo = SyncTempo::no;
    SyncPitch syncPitch = SyncPitch::no;
    std::shared_ptr<tempo::Sequence> chordPitchSequence;
    std::shared_ptr<tempo::Sequence::Position> chordPitchPosition;

    bool buildAudioReaderGraph();
    void replaceStateIfPossible (NodeGraph*);
    void replaceStateIfPossible (WaveNodeRealTime&);
    void processSection (ProcessContext&);
    tempo::Key getKeyToSyncTo (TimePosition) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveNodeRealTime)
};

}} // namespace tracktion { inline namespace engine

#ifndef DOXYGEN
template<>
struct std::hash<tracktion::engine::WarpMap>
{
    size_t operator() (const tracktion::engine::WarpMap& w) const noexcept
    {
        size_t seed = 0;

        for (const auto& p : w)
        {
            tracktion::hash_combine (seed, p.sourceTime.inSeconds());
            tracktion::hash_combine (seed, p.warpTime.inSeconds());
        }

        return seed;
    }
};
#endif
