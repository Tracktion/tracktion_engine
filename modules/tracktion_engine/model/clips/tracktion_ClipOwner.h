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

    /** Called when a clip is deleted which could be during Edit destruction or track deletion etc. */
    virtual void clipDeleted (Clip&) = 0;

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


}} // namespace tracktion { inline namespace engine
