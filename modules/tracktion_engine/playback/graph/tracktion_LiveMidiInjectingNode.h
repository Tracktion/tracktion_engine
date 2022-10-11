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
/**
    A Node that injects MIDI message in to the stream, for keyboard key entry,
    note previews and MIDI step entry etc.
*/
class LiveMidiInjectingNode final   : public tracktion::graph::Node,
                                      private AudioTrack::Listener
{
public:
    LiveMidiInjectingNode (AudioTrack&, std::unique_ptr<tracktion::graph::Node>);
    ~LiveMidiInjectingNode() override;

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    AudioTrack::Ptr track;
    std::unique_ptr<tracktion::graph::Node> input;

    juce::CriticalSection liveMidiLock;
    MidiMessageArray liveMidiMessages;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    //==============================================================================
    void injectMessage (MidiMessageArray::MidiMessageWithSource);

    //==============================================================================
    void injectLiveMidiMessage (AudioTrack&, const MidiMessageArray::MidiMessageWithSource&, bool& wasUsed) override;
    void recordedMidiMessageSentToPlugins (AudioTrack&, const juce::MidiMessage&) override {}
};

}} // namespace tracktion { inline namespace engine
