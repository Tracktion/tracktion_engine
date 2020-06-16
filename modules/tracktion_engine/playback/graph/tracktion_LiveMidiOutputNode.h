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
/**
    A Node that calls the listeners of an AudioTrack with any incomming MIDI.
*/
class LiveMidiOutputNode final : public tracktion_graph::Node,
                                 private juce::AsyncUpdater
{
public:
    LiveMidiOutputNode (AudioTrack&, std::unique_ptr<tracktion_graph::Node>);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (const ProcessContext&) override;

private:
    //==============================================================================
    AudioTrack& track;
    Track::Ptr trackPtr;
    std::unique_ptr<tracktion_graph::Node> input;

    juce::CriticalSection lock;
    MidiMessageArray pendingMessages, dispatchingMessages;

    //==============================================================================
    void handleAsyncUpdate() override;
};

} // namespace tracktion_engine
