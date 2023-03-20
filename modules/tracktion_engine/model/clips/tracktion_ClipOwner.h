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


//==============================================================================
//==============================================================================
/** Determines behaviour for overwriting clips. */
enum class DeleteExistingClips
{
    no, /*<< Don't remove existing clips. */
    yes /*<< Replace existing clips with new ones. */
};

/** Inserts a clip with the given state in to the ClipOwner's clip list. */
Clip* insertClipWithState (ClipOwner&, juce::ValueTree);

/** Inserts a clip with the given state in to the ClipOwner's clip list. */
Clip* insertClipWithState (ClipOwner&,
                           const juce::ValueTree& stateToUse, const juce::String& name, TrackItem::Type,
                           ClipPosition, DeleteExistingClips, bool allowSpottingAdjustment);

//==============================================================================
/** Inserts a new clip with the given type and a default name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, TimeRange);

/** Inserts a new clip with the given type and name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, const juce::String& name, TimeRange);

/** Inserts a new clip with the given type and name. */
Clip* insertNewClip (ClipOwner&, TrackItem::Type, const juce::String& name, ClipPosition);

//==============================================================================
/** Inserts a new WaveAudioClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (ClipOwner&, const juce::String& name, const juce::File& sourceFile,
                                                               ClipPosition, DeleteExistingClips);

/** Inserts a new WaveAudioClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (ClipOwner&, const juce::String& name, ProjectItemID sourceID,
                                                               ClipPosition, DeleteExistingClips);

/** Inserts a new MidiClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (ClipOwner&, const juce::String& name, TimeRange);

/** Inserts a new MidiClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (ClipOwner&, TimeRange);

/** Inserts a new EditClip into the ClipOwner's clip list. */
juce::ReferenceCountedObjectPtr<EditClip> insertEditClip (ClipOwner&, TimeRange, ProjectItemID);

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

}} // namespace tracktion { inline namespace engine
