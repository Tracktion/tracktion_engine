/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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
                         tracktion::graph::PlayHeadState&,
                         EditItemID targetID);
    ~MidiInputDeviceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

    void handleIncomingMidiMessage (const juce::MidiMessage&) override;
    void discardRecordings (EditItemID) override;

private:
    //==============================================================================
    InputDeviceInstance& instance;
    MidiInputDevice& midiInputDevice;
    const MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::notMPE;
    tracktion::graph::PlayHeadState& playHeadState;
    const EditItemID targetID;
    const size_t nodeID = hash ((size_t) midiSourceID, targetID);

    unsigned int maxExpectedMsPerBuffer = 0;
    double sampleRate = 44100.0;

    LambdaTimer loopOverdubsChecker { [this] { updateLoopOverdubs(); } };

    struct NodeState
    {
        NodeState()
        {
            for (int i = 256; --i >= 0;)
                incomingMessages.add (new juce::MidiMessage (0x80, 0, 0));
        }

        std::atomic<MidiInputDeviceNode*> activeNode { nullptr };

        std::mutex incomingMessagesMutex;
        juce::OwnedArray<juce::MidiMessage> incomingMessages;
        int numIncomingMessages = 0;

        std::mutex liveMessagesMutex;
        MidiMessageArray liveRecordedMessages;
        int numLiveMessagesToPlay = 0; // the index of the first message that's been recorded in the current loop

        juce::uint32 lastReadTime = juce::Time::getApproximateMillisecondCounter();
        double lastPlayheadTime = 0;
        std::atomic<bool> canLoop { true };
    };

    std::shared_ptr<NodeState> state;

    //==============================================================================
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);
    void createProgramChanges (MidiMessageArray&);
    bool isLivePlayOverActive();
    void updateLoopOverdubs();
};

}} // namespace tracktion { inline namespace engine
