/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

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

    /** Must return the ID of this ClipOwner. */
    virtual EditItemID getClipOwnerID() = 0;

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


//==============================================================================
//==============================================================================
/** Determines behaviour for overwriting clips. */
enum class DeleteExistingClips
{
    no, /*<< Don't remove existing clips. */
    yes /*<< Replace existing clips with new ones. */
};

/** Creates a clip with the given state in the ClipOwner's clip list.

    The state is treated as a brand new clip, so the defaults for a newly created
    clip are applied to it and EngineBehaviour::newClipCreated is called. If the
    owner is a ClipSlot the clip is also prepared for the launcher.

    To move or copy a clip that already exists, use Clip::moveTo or insertClipCopy
    so the clip keeps the settings it has.
*/
Clip* insertClipWithState (ClipOwner&, juce::ValueTree);

/** Creates a clip with the given state in the ClipOwner's clip list.
    @see insertClipWithState (ClipOwner&, juce::ValueTree)
*/
Clip* insertClipWithState (ClipOwner&,
                           const juce::ValueTree& stateToUse, const juce::String& name, TrackItem::Type,
                           ClipPosition, DeleteExistingClips, bool allowSpottingAdjustment);

//==============================================================================
/** Holds a copy of an existing clip's state, along with whether that clip was in
    the launcher.

    Inserting one of these copies the clip as it is: none of the defaults for a
    newly created clip are applied, so the copy keeps the settings the user gave
    the original. A clip that comes from outside the launcher is still prepared
    for the launcher when it's inserted in to a ClipSlot.

    There's deliberately no way to build one of these from an arbitrary state -
    it can only describe a clip that already exists.
*/
class ClipCopy
{
public:
    /** Takes a copy of a live clip's state.
        The caller is responsible for giving the copy new EditItemIDs if the
        original is going to stay in the Edit.
    */
    static ClipCopy fromClip (const Clip&);

    /** Describes a clip state that came from the clipboard.
        @param stateToUse           The clip state, with its IDs already remapped
        @param sourceWasInLauncher  Whether the clip it was copied from was in a ClipSlot
    */
    static ClipCopy fromClipboardState (juce::ValueTree stateToUse, bool sourceWasInLauncher);

    /** Returns a copy with a new EditItemID, for when the clip it was taken from
        is staying in the Edit.
    */
    ClipCopy withNewItemID (Edit&) const;

    /** Returns the state that will be inserted. */
    const juce::ValueTree& getState() const                 { return state; }

    /** Returns true if the clip this was taken from was in a ClipSlot. */
    bool wasInLauncher() const                              { return sourceWasInLauncher; }

private:
    ClipCopy (juce::ValueTree, bool);

    juce::ValueTree state;
    bool sourceWasInLauncher = false;
};

/** Inserts a copy of an existing clip in to the ClipOwner's clip list.
    @see ClipCopy
*/
Clip* insertClipCopy (ClipOwner&, const ClipCopy&);

/** Applies the settings a clip needs to be used in the launcher: no clip effects
    or proxy, starting at 0 and looping over its length.

    This is done for clips that are created in a ClipSlot and for clips that move
    in to one from the arrangement. Clips that are already in the launcher keep
    their own settings.
*/
void prepareClipForLauncher (Clip&);

//==============================================================================
/** Inserts a new clip with the given type and a default name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, EditTimeRange);

/** Inserts a new clip with the given type and name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, const juce::String& name, EditTimeRange);

/** Inserts a new clip with the given type and name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, const juce::String& name, ClipPosition);

//==============================================================================
/** Inserts a new WaveAudioClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (ClipOwner&, const juce::String& name, const juce::File& sourceFile,
                                                               ClipPosition, DeleteExistingClips);

/** Inserts a new WaveAudioClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (ClipOwner&, const juce::String& name, ProjectItemRef sourceID,
                                                               ClipPosition, DeleteExistingClips);

/** Inserts a new MidiClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (ClipOwner&, const juce::String& name, TimeRange);

/** Inserts a new MidiClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (ClipOwner&, TimeRange);

/** Inserts a new EditClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<EditClip> insertEditClip (ClipOwner&, TimeRange, ProjectItemRef);

//==============================================================================
/** Removes a region of a ClipOwner and returns any newly created clips. */
juce::Array<Clip*> deleteRegion (ClipOwner&, TimeRange);

/** Removes a region of a clip and returns any newly created clips. */
juce::Array<Clip*> deleteRegion (Clip&, TimeRange);

/** Splits the given clp owner at the time and returns any newly created clips. */
juce::Array<Clip*> split (ClipOwner&, TimePosition);

/** Splits the given clip at the time and returns the newly created clip. */
Clip* split (Clip&, TimePosition);

/** Returns true if the clip owner contains any MIDI clips. */
[[ nodiscard ]] bool containsAnyMIDIClips (const ClipOwner&);

/** Returns the subclips of the given type. */
template<typename ClipType>
[[ nodiscard ]] juce::Array<ClipType*> getClipsOfType (const ClipOwner&);

/** Returns the subclips of the given type, if any clips contain other clips, this will also return those. */
template<typename ClipType>
[[ nodiscard ]] juce::Array<ClipType*> getClipsOfTypeRecursive (const ClipOwner&);


//==============================================================================
//==============================================================================
/** Returns true if this is a MasterTrack. */
bool isMasterTrack (const Track&);

/** Returns true if this is a TempoTrack. */
bool isTempoTrack (const Track&);

/** Returns true if this is an AutomationTrack. */
bool isAutomationTrack (const Track&);

/** Returns true if this is an AudioTrack. */
bool isAudioTrack (const Track&);

/** Returns true if this is a FolderTrack. */
bool isFolderTrack (const Track&);

/** Returns true if this is a MarkerTrack. */
bool isMarkerTrack (const Track&);

/** Returns true if this is a ChordTrack. */
bool isChordTrack (const Track&);

/** Returns true if this is an ArrangerTrack. */
bool isArrangerTrack (const Track&);

/** Returns true if this is an AudioTrack. */
bool isAudioTrack (const ClipOwner&);

/** Returns true if this is a FolderTrack. */
bool isFolderTrack (const ClipOwner&);

/** Returns true if this is a MarkerTrack. */
bool isMarkerTrack (const ClipOwner&);

/** Returns true if this is a ChordTrack. */
bool isChordTrack (const ClipOwner&);

/** Returns true if this is an ArrangerTrack. */
bool isArrangerTrack (const ClipOwner&);

//==============================================================================
/** Returns true if this Track can contain MidiClip[s]. */
bool canContainMIDI (const ClipOwner&);

/** Returns true if this Track can contain WaveAudioClip[s]. */
bool canContainAudio (const ClipOwner&);

/** Returns true if this Track is movable. @see AudioTrack, FolderTrack */
bool isMovable (const Track&);

/** Returns true if this a global Track and should be on top of others. @see MarkerTrack, TempoTrack */
bool isOnTop (const Track&);

//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

template<typename ClipType>
inline juce::Array<ClipType*> getClipsOfType (const ClipOwner& parent)
{
    juce::Array<ClipType*> clips;

    for (auto clip : parent.getClips())
        if (auto typedClip = dynamic_cast<ClipType*> (clip))
            clips.add (typedClip);

    return clips;
}

template<typename ClipType>
inline juce::Array<ClipType*> getClipsOfTypeRecursive (const ClipOwner& parent)
{
    juce::Array<ClipType*> results;

    results.addArray (getClipsOfType<ClipType> (parent));

    for (auto clip : parent.getClips())
        if (auto clipOwner = dynamic_cast<ClipOwner*> (clip))
            results.addArray (getClipsOfTypeRecursive<ClipType> (*clipOwner));

    return results;
}

} // namespace tracktion::inline engine
