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

/** A Node that plays MIDI data from a MidiMessageSequence,
    at a specific MIDI channel.
*/
class MidiNode final    : public tracktion_graph::Node
{
public:
    MidiNode (juce::MidiMessageSequence,
              juce::Range<int> midiChannelNumbers,
              bool useMPE,
              EditTimeRange editSection,
              LiveClipLevel,
              tracktion_graph::PlayHeadState&,
              EditItemID,
              std::function<bool()> shouldBeMutedDelegate = nullptr);
    
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (const ProcessContext&) override;

private:
    //==============================================================================
    juce::MidiMessageSequence ms;
    juce::Range<int> channelNumbers;
    bool useMPEChannelMode;
    EditTimeRange editSection;
    LiveClipLevel clipLevel;
    tracktion_graph::PlayHeadState& playHeadState;
    EditItemID editItemID;
    std::function<bool()> shouldBeMutedDelegate = nullptr;
    
    double sampleRate = 44100.0;
    int currentIndex = 0;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();
    bool wasMute = false, shouldCreateMessagesForTime = false;

    juce::Array<juce::MidiMessage> controllerMessagesScratchBuffer;

    //==============================================================================
    void createMessagesForTime (double time, MidiMessageArray&);
    void createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& source,
                         double time, double midiTimeOffset, bool isPlaying);
    void processSection (const ProcessContext&, juce::Range<int64_t> timelineRange);
};

} // namespace tracktion_engine
