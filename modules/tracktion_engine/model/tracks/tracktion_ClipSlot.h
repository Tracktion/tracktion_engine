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
    Represents a slot on a track that a Clip can live in to be played as a launched clip.
*/
class ClipSlot : public EditItem,
                 public Selectable,
                 public ClipOwner
{
public:
    /** Creates a ClipSlot for a given Track. */
    ClipSlot (const juce::ValueTree&, Track&);
    /** Destructor. */
    ~ClipSlot() override;

    //==============================================================================
    /** Sets the Clip to be used within this slot. */
    void setClip (Clip*);

    /** Returns the currently set clip. */
    Clip* getClip();

    /** Returns slot index on track */
    int getIndex();

    InputDeviceInstance::Destination* getInputDestination();

    //==============================================================================
    /** @internal. */
    juce::String getName() const override;
    /** @internal. */
    juce::String getSelectableDescription() override;
    /** @internal. */
    juce::ValueTree& getClipOwnerState() override;
    /** @internal. */
    EditItemID getClipOwnerID() override;
    /** @internal. */
    Selectable* getClipOwnerSelectable() override;
    /** @internal. */
    Edit& getClipOwnerEdit() override;

    juce::ValueTree state;    /**< The state of this ClipSlot. */
    Track& track;             /**< The Track this ClipSlot belongs to. */

private:
    void clipCreated (Clip&) override;
    void clipAddedOrRemoved() override;
    void clipOrderChanged() override;
    void clipPositionChanged() override;
};


//==============================================================================
//==============================================================================
/**
    A list of the ClipSlots on a Track.
*/
class ClipSlotList  : private ValueTreeObjectList<ClipSlot>
{
public:
    /** Creates a ClipSlotList for a Track. */
    ClipSlotList (const juce::ValueTree&, Track&);
    /** Destructor. */
    ~ClipSlotList() override;

    /** Returns the ClipSlot* on this Track. */
    juce::Array<ClipSlot*> getClipSlots() const;

    /** Adds Slots to ensure at least numSlots exist. */
    void ensureNumberOfSlots (int numSlots);

    /** Adds or removes Slots to ensure the exact number of slots exist. */
    void setNumberOfSlots (int numSlots);

    /** Inserts a new slot with the given index. If the index is < 0 or greater
        than the current number of scenes, the new one will be added at the end of the list
     */
    ClipSlot* insertSlot (int index);

    juce::ValueTree state;  /**< The state of this ClipSlotList. */
    Track& track;           /**< The Track this ClipSlotList belongs to. */

    /** @internal. */
    void deleteSlot (ClipSlot&);

private:
    bool isSuitableType (const juce::ValueTree&) const override;
    ClipSlot* createNewObject (const juce::ValueTree&) override;
    void deleteObject (ClipSlot*) override;

    void newObjectAdded (ClipSlot*) override;
    void objectRemoved (ClipSlot*) override;
    void objectOrderChanged() override;
};

}} // namespace tracktion { inline namespace engine
