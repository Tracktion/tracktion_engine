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
class MidiNode final    : public tracktion_graph::Node,
                          public TracktionEngineNode
{
public:
    MidiNode (juce::MidiMessageSequence sequence,
              juce::Range<int> midiChannelNumbers,
              bool useMPE,
              EditTimeRange editSection,
              LiveClipLevel,
              ProcessState&,
              EditItemID,
              std::function<bool()> shouldBeMutedDelegate = nullptr);

    MidiNode (std::vector<juce::MidiMessageSequence> sequences,
              juce::Range<int> midiChannelNumbers,
              bool useMPE,
              EditTimeRange editSection,
              LiveClipLevel,
              ProcessState&,
              EditItemID,
              std::function<bool()> shouldBeMutedDelegate = nullptr);
    
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::vector<juce::MidiMessageSequence> ms;
    int64_t lastStart = 0;
    size_t currentSequence = 0;
    juce::Range<int> channelNumbers;
    bool useMPEChannelMode;
    EditTimeRange editSection;
    LiveClipLevel clipLevel;
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
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);
};

} // namespace tracktion_engine
