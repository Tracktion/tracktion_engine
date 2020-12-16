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
/** Node that reads from a file node and timestretches its output.

    Annoyingly, this has to replicate a lot of the functionality in WaveNode.
    Note that this isn't designed to be a fully fledged AudioNode, it doesn't deal
    with clip offsets, loops etc. It's only designed for previewing files.

    A cleaner approach might be to take a WaveAudioNode and stretch the contents.
*/
class TimeStretchingWaveNode final : public tracktion_graph::Node
{
public:
    TimeStretchingWaveNode (AudioClipBase&, tracktion_graph::PlayHeadState&);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    AudioClipBase& c;
    tracktion_graph::PlayHeadState& playHeadState;
    Clip::Ptr clipPtr;

    AudioFile file;
    AudioFileInfo fileInfo;
    AudioFileCache::Reader::Ptr reader;

    double sampleRate = 44100.0, fileSpeedRatio = 1.0, nextEditTime = -1.0;
    TimeStretcher timestretcher;
    float speedRatio = 1.0f, pitchSemitones = 0;
    float timestetchSpeedRatio = 1.0f, timestetchSemitonesUp = 1.0f;

    tracktion_graph::AudioFifo fifo;
    int stretchBlockSize = 512;

    //==============================================================================
    int64_t timeToFileSample (double) const noexcept;
    void reset (double newStartTime);
    bool fillNextBlock();
};

} // namespace tracktion_engine
