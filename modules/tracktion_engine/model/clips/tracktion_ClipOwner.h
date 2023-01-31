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
    Base class for items that can contain clips.
    @see ClipTrack, ContainerClip
*/
class ClipOwner
{
public:
    /** Constructs an empty ClipOwner.
        Call initialiseClipOwner in the subclass constructor to initialise it.
    */
    ClipOwner();

    /** Destructor. */
    virtual ~ClipOwner();

    /** Must return the state of this ClipOwner. */
    virtual juce::ValueTree& getClipOwnerState() = 0;

    /** Must return the selectable if this ClipOwner is one. */
    virtual Selectable* getClipOwnerSelectable() = 0;

    /** Must return the Edit this ClipOwner belongs to. */
    virtual Edit& getClipOwnerEdit() = 0;

    /** Returns the clips this owner contains. */
    const juce::Array<Clip*>& getClips() const;

protected:
    /** Must be called once from the subclass constructor to init the clip owner. */
    void initialiseClipOwner (Edit&, juce::ValueTree clipParentState);

    /** Called when a clip is created which could be during Edit load. */
    virtual void clipCreated (Clip&) = 0;

    /** Called when a clip is added or removed.
        This is subtly different to the created/deleted callback as it will only
        get called whilst the Edit is in normal operation.
    */
    virtual void clipAddedOrRemoved() = 0;

    /** Called when clips have moved times so that their order has changed.
        N.B. This may be asyncronously to their start times changing.
    */
    virtual void clipOrderChanged() = 0;

    /** Called when a clip start or end position has changed. */
    virtual void clipPositionChanged() = 0;

private:
    struct ClipList;
    std::unique_ptr<ClipList> clipList;
};

//==============================================================================
/** Returns a clip with the given state if the ClipOwner contains it. */
Clip* findClipForState (ClipOwner&, const juce::ValueTree&);

/** Returns a clip with the given ID if the ClipOwner contains it. */
Clip* findClipForID (ClipOwner&, EditItemID);

/** Inserts a clip with the given state in to the ClipOwner's clip list*/
Clip* insertClipWithState (ClipOwner&, juce::ValueTree);

//==============================================================================
//==============================================================================
/** Returns true if this is an AudioTrack. */
bool isAudioTrack (ClipOwner&);

/** Returns true if this is an AutomationTrack. */
bool isAutomationTrack (ClipOwner&);

/** Returns true if this is a FolderTrack. */
bool isFolderTrack (ClipOwner&);

/** Returns true if this is a MarkerTrack. */
bool isMarkerTrack (ClipOwner&);

/** Returns true if this is a TempoTrack. */
bool isTempoTrack (ClipOwner&);

/** Returns true if this is a ChordTrack. */
bool isChordTrack (ClipOwner&);

/** Returns true if this is an ArrangerTrack. */
bool isArrangerTrack (ClipOwner&);

/** Returns true if this is a MasterTrack. */
bool isMasterTrack (ClipOwner&);

//==============================================================================
/** Returns true if this Track can contain MarkerClip[s]. */
bool canContainMarkers (ClipOwner&);

/** Returns true if this Track can contain MidiClip[s]. */
bool canContainMIDI (ClipOwner&);

/** Returns true if this Track can contain WaveAudioClip[s]. */
bool canContainAudio (ClipOwner&);

/** Returns true if this Track can contain EditClip[s]. */
bool canContainEditClips (ClipOwner&);

/** Returns true if this Track can contain Plugin[s]. */
bool canContainPlugins (ClipOwner&);

/** Returns true if this Track is movable. @see AudioTrack, FolderTrack */
bool isMovable (ClipOwner&);

/** Returns true if this a global Track and should be on top of others. @see MarkerTrack, TempoTrack */
bool isOnTop (ClipOwner&);

/** Returns true if this track can have inputs assigned to it. @see AudioTrack */
bool acceptsInput (ClipOwner&);

/** Returns true if this track creates audible output. @see AudioTrack */
bool createsOutput (ClipOwner&);

/** Returns true if this track can show automation. @see AudioTrack, FolderTrack, AutomationTrack */
bool wantsAutomation (ClipOwner&);


}} // namespace tracktion { inline namespace engine
