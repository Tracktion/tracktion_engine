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

/**
    A Node that handles muting/soloing of its input node, according to
    the audibility of a track.
*/
class TrackMutingNode final : public tracktion_graph::Node
{
public:
    TrackMutingNode (Track&, std::unique_ptr<tracktion_graph::Node> input,
                     bool muteForInputsWhenRecording, bool processMidiWhenMuted);

    TrackMutingNode (Edit&, std::unique_ptr<tracktion_graph::Node> input);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override {}
    bool isReadyToProcess() override;
    void process (const ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<tracktion_graph::Node> input;
    Edit& edit;
    juce::ReferenceCountedObjectPtr<Track> track;
    bool wasBeingPlayed = false;
    bool callInputWhileMuted = false;
    bool processMidiWhileMuted = false;
    juce::Array<InputDeviceInstance*> inputDevicesToMuteFor;

    //==============================================================================
    bool isBeingPlayed() const;
    bool wasJustMuted (bool isPlayingNow) const;
    bool wasJustUnMuted (bool isPlayingNow) const;
    void updateLastMutedState (bool isPlayingNow);

    void rampBlock (juce::dsp::AudioBlock<float>&, float start, float end);
    void sendAllNotesOffIfDesired (MidiMessageArray&);
};

} // namespace tracktion_engine
