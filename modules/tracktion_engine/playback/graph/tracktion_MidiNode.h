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

/** A Node that plays MIDI data from a MidiMessageSequence,
    at a specific MIDI channel.
*/
//dddclass MidiNode final    : public tracktion::graph::Node,
//                          public TracktionEngineNode
//{
//public:
//    MidiNode (juce::MidiMessageSequence sequence,
//              juce::Range<int> midiChannelNumbers,
//              bool useMPE,
//              TimeRange editSection,
//              LiveClipLevel,
//              ProcessState&,
//              EditItemID,
//              std::function<bool()> shouldBeMutedDelegate = nullptr);
//
//    MidiNode (std::vector<juce::MidiMessageSequence> sequences,
//              juce::Range<int> midiChannelNumbers,
//              bool useMPE,
//              TimeRange editSection,
//              LiveClipLevel,
//              ProcessState&,
//              EditItemID,
//              std::function<bool()> shouldBeMutedDelegate = nullptr);
//
//    tracktion::graph::NodeProperties getNodeProperties() override;
//    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
//    bool isReadyToProcess() override;
//    void process (ProcessContext&) override;
//
//private:
//    //==============================================================================
//    std::vector<juce::MidiMessageSequence> ms;
//    int64_t lastStart = 0;
//    size_t currentSequence = 0;
//    juce::Range<int> channelNumbers;
//    bool useMPEChannelMode;
//    TimeRange editSection;
//    LiveClipLevel clipLevel;
//    EditItemID editItemID;
//    std::function<bool()> shouldBeMutedDelegate = nullptr;
//
//    double sampleRate = 44100.0;
//    TimeDuration timeForOneSample;
//    int currentIndex = 0;
//    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();
//    bool wasMute = false, shouldCreateMessagesForTime = false;
//
//    juce::Array<juce::MidiMessage> controllerMessagesScratchBuffer;
//
//    //==============================================================================
//    void createMessagesForTime (TimePosition, MidiMessageArray&);
//    void createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& source,
//                         TimePosition, TimeDuration midiTimeOffset, bool isPlaying);
//    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);
//};


//==============================================================================
//==============================================================================
/** A Node that plays MIDI data from a MidiMessageSequence,
    at a specific MIDI channel.
*/
class MidiNode final    : public tracktion::graph::Node,
                          public TracktionEngineNode
{
public:
    MidiNode (std::vector<juce::MidiMessageSequence> sequences,
              MidiList::TimeBase,
              juce::Range<int> midiChannelNumbers,
              bool useMPE,
              juce::Range<double> editSection,
              LiveClipLevel,
              ProcessState&,
              EditItemID,
              std::function<bool()> shouldBeMutedDelegate = nullptr);

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::vector<juce::MidiMessageSequence> ms;
    const MidiList::TimeBase timeBase;
    int64_t lastStart = 0;
    size_t currentSequence = 0;
    juce::Range<int> channelNumbers;
    bool useMPEChannelMode;
    juce::Range<double> editRange;
    LiveClipLevel clipLevel;
    EditItemID editItemID;
    std::function<bool()> shouldBeMutedDelegate = nullptr;

    double sampleRate = 44100.0;
    int currentIndex = 0;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();
    bool wasMute = false, shouldCreateMessagesForTime = false;

    juce::Array<juce::MidiMessage> controllerMessagesScratchBuffer;

    //==============================================================================
    void processSection (ProcessContext&, juce::Range<int64_t> timelineRange);
    void processSection (Node::ProcessContext&,
                         juce::Range<double> sectionEditTime,
                         double secondsPerTimeBase,
                         juce::MidiMessageSequence&);
};

}} // namespace tracktion { inline namespace engine
