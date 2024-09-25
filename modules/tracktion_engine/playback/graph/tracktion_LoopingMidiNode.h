/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

class GeneratorAndNoteList;

//==============================================================================
//==============================================================================
/** A Node that plays MIDI data from a MidiMessageSequence,
    at a specific MIDI channel.
    N.B. In order to loop correctly at changing tempos, the sequence
    time base must be in beats.
*/
class LoopingMidiNode final : public tracktion::graph::Node,
                              public TracktionEngineNode,
                              public DynamicallyOffsettableNodeBase
{
public:
    LoopingMidiNode (std::vector<juce::MidiMessageSequence> sequences,
                     juce::Range<int> midiChannelNumbers,
                     bool useMPE,
                     BeatRange editRange,
                     BeatRange loopRange,
                     BeatDuration offset,
                     LiveClipLevel,
                     ProcessState&,
                     EditItemID,
                     const QuantisationType&,
                     const GrooveTemplate*,
                     float grooveStrength,
                     std::function<bool()> shouldBeMutedDelegate = nullptr);

    /** Returns the EditItemID to identify this Node. */
    EditItemID getItemID() const                            { return editItemID; }

    /** Returns the MPESourceID used by this Node. */
    MPESourceID getMPESourceID() const    { return midiSourceID; }

    /** Returns the ActiveNoteList in use for this Node.
        Call this after prepareToPlay to get the one in use.
    */
    const std::shared_ptr<ActiveNoteList>& getActiveNoteList() const;

    //==============================================================================
    /** Sets an offset to be applied to all times in this node, effectively shifting
        it forwards or backwards in time.
    */
    void setDynamicOffsetBeats (BeatDuration) override;

    /** Iterates the ActiveNoteList adding note-off events for the active notes and
        then resets them.
    */
    void killActiveNotes (MidiMessageArray&, double timestampForNoteOffs);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<GeneratorAndNoteList> generatorAndNoteList;
    juce::Range<int> channelNumbers;
    bool useMPEChannelMode;
    const BeatRange editRange;
    LiveClipLevel clipLevel;
    EditItemID editItemID;
    std::function<bool()> shouldBeMutedDelegate = nullptr;
    std::shared_ptr<BeatDuration> dynamicOffsetBeats = std::make_shared<BeatDuration>();

    MPESourceID midiSourceID = createUniqueMPESourceID();
    bool wasMute = false;
};

}} // namespace tracktion { inline namespace engine
