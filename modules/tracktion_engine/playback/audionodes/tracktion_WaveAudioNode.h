/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_AudioNode.h"


namespace tracktion { inline namespace engine
{

//==============================================================================
/** An AudioNode that plays back a wave file. */
class WaveAudioNode  : public AudioNode
{
public:
    /** offset is a time added to the start of the file, e.g. an offset of 10.0
        would produce ten seconds of silence followed by the file.

        gain may be 0, or a pointer to a floating point value which is referred to as the gain
        to use when converting the file contents to floating point. e.g. gain of
        2.0f will double the values returned.

    */
    WaveAudioNode (const AudioFile& file,
                   legacy::EditTimeRange editTime,
                   double offset,
                   legacy::EditTimeRange loopSection,
                   LiveClipLevel level,
                   double speedRatio,
                   const juce::AudioChannelSet& channelsToUse);

    ~WaveAudioNode() override;

    //==============================================================================
    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToRender() override;
    void releaseAudioNodeResources() override;
    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;
    void prepareForNextBlock (const AudioRenderContext&) override;

    void renderSection (const AudioRenderContext&, legacy::EditTimeRange editTime);

private:
    //==============================================================================
    legacy::EditTimeRange editPosition, loopSection;
    double offset = 0;
    double originalSpeedRatio = 0, outputSampleRate = 44100.0;

    AudioFile audioFile;
    LiveClipLevel clipLevel;
    double audioFileSampleRate = 0;
    const juce::AudioChannelSet channelsToUse;
    AudioFileCache::Reader::Ptr reader;

    struct PerChannelState;
    juce::OwnedArray<PerChannelState> channelState;

    SampleCount editTimeToFileSample (double) const noexcept;
    bool updateFileSampleRate();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveAudioNode)
};

}} // namespace tracktion { inline namespace engine
