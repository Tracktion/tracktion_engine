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

/**
    A Node that intercepts incoming live MIDI and inserts it in to the playback graph.
*/
class MidiInputDeviceNode final : public tracktion::graph::Node,
                                  public InputDeviceInstance::Consumer
{
public:
    MidiInputDeviceNode (InputDeviceInstance&,
                         MidiInputDevice&, MidiMessageArray::MPESourceID,
                         tracktion::graph::PlayHeadState&);
    ~MidiInputDeviceNode() override;
    
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

    void handleIncomingMidiMessage (const juce::MidiMessage&) override;
    void discardRecordings() override;

private:
    //==============================================================================
    InputDeviceInstance& instance;
    MidiInputDevice& midiInputDevice;
    const  MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::notMPE;
    tracktion::graph::PlayHeadState& playHeadState;

    juce::CriticalSection bufferLock;
    juce::OwnedArray<juce::MidiMessage> incomingMessages;
    int numMessages = 0;
    MidiMessageArray liveRecordedMessages;
    int numLiveMessagesToPlay = 0; // the index of the first message that's been recorded in the current loop
    juce::CriticalSection liveInputLock;
    unsigned int lastReadTime = 0, maxExpectedMsPerBuffer = 0;
    double sampleRate = 44100.0, lastPlayheadTime = 0;

    //==============================================================================
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);
    void createProgramChanges (MidiMessageArray&);
    bool isLivePlayOverActive();
};

}} // namespace tracktion { inline namespace engine
