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
    Holds the state of a Track and if its contents/plugins should be played or not.
*/
class TrackMuteState
{
public:
    /** Creates a TrackMuteState for a Track. */
    TrackMuteState (Track&, bool muteForInputsWhenRecording, bool processMidiWhenMuted);

    /** Creates a TrackMuteState for an Edit.
        This is used for freeze tracks and will be muted if any other track is soloed.
    */
    TrackMuteState (Edit&);

    /** Call once per block to update the mute status. */
    void update();
    
    //==============================================================================
    /** Returns true if the track's mix bus should be audible. */
    bool shouldTrackBeAudible() const;

    /** Returns true if the track's contents should be processed e.g. clips and plugins. */
    bool shouldTrackContentsBeProcessed() const;

    /** Returns true if the track's MIDI should be processed to avoid breaks in long notes. */
    bool shouldTrackMidiBeProcessed() const;

    //==============================================================================
    /** Returns true if the last block was audible but this one isn't. */
    bool wasJustMuted() const       { return wasJustMutedFlag.load (std::memory_order_acquire); }

    /** Returns true if the last block wasn't audible but this one is. */
    bool wasJustUnMuted() const     { return wasJustUnMutedFlag.load (std::memory_order_acquire); }

    //==============================================================================
    /** Returns the ID for the relevant item, either the Track or Edit. */
    size_t getItemID() const;

private:
    //==============================================================================
    Edit& edit;
    juce::ReferenceCountedObjectPtr<Track> track;
    bool wasBeingPlayedFlag = false;
    std::atomic<bool> wasJustMutedFlag { false }, wasJustUnMutedFlag { false };
    
    bool callInputWhileMuted = false;
    bool processMidiWhileMuted = false;
    juce::Array<InputDeviceInstance*> inputDevicesToMuteFor;

    //==============================================================================
    bool isBeingPlayed() const;
};


//==============================================================================
//==============================================================================
/**
    A Node that handles muting/soloing of its input node, according to
    the audibility of a track.
*/
class TrackMutingNode final : public tracktion::graph::Node
{
public:
    TrackMutingNode (std::unique_ptr<TrackMuteState>, std::unique_ptr<tracktion::graph::Node> input,
                     bool dontMuteIfTrackContentsShouldBeProcessed);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override {}
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t>) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<TrackMuteState> trackMuteState;
    std::unique_ptr<tracktion::graph::Node> input;
    bool dontMuteIfTrackContentsShouldBeProcessed = false;

    //==============================================================================
    void rampBlock (choc::buffer::ChannelArrayView<float>, float start, float end);
    void sendAllNotesOffIfDesired (MidiMessageArray&);
};

}} // namespace tracktion { inline namespace engine
