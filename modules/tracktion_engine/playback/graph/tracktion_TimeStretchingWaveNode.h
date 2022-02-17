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
/** Node that reads from a file node and timestretches its output.

    Annoyingly, this has to replicate a lot of the functionality in WaveNode.
    Note that this isn't designed to be a fully fledged Node, it doesn't deal
    with clip offsets, loops etc. It's only designed for previewing files.
*/
class TimeStretchingWaveNode final : public tracktion::graph::Node
{
public:
    TimeStretchingWaveNode (AudioClipBase&, tracktion::graph::PlayHeadState&);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    AudioClipBase& c;
    tracktion::graph::PlayHeadState& playHeadState;
    Clip::Ptr clipPtr;

    AudioFile file;
    AudioFileInfo fileInfo;
    AudioFileCache::Reader::Ptr reader;

    double sampleRate = 44100.0, fileSpeedRatio = 1.0, nextEditTime = -1.0;
    TimeStretcher timestretcher;
    float speedRatio = 1.0f, pitchSemitones = 0;
    float timestetchSpeedRatio = 1.0f, timestetchSemitonesUp = 1.0f;

    tracktion::graph::AudioFifo fifo;
    int stretchBlockSize = 512;

    //==============================================================================
    int64_t timeToFileSample (double) const noexcept;
    void reset (double newStartTime);
    bool fillNextBlock();
};

}} // namespace tracktion { inline namespace engine
