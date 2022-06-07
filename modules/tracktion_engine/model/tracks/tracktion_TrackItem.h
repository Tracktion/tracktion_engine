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

//==============================================================================
/**
    Base class for EditItems that live in a Track, e.g. clips
    @see WaveAudioClip, MidiClip, StepClip, MarkerClip, PitchSetting, TimeSigSetting
*/
class TrackItem   : public EditItem,
                    public Selectable,
                    public juce::ReferenceCountedObject
{
public:
    /** Defines the types of item that can live on Track[s]. */
    enum class Type
    {
        unknown,    /**< A placeholder for unknown items. */
        wave,       /**< A wave clip. @see WaveAudioClip */
        midi,       /**< A MIDI clip. @see MidiClip. */
        edit,       /**< An Edit clip. @see EditClip. */
        step,       /**< A MIDI step clip. @see StepClip. */
        marker,     /**< A marker clip. @see MarkerClip. */
        pitch,      /**< A pitch setting. @see PitchSetting. */
        timeSig,    /**< A time signature settings. @see TimeSigSetting. */
        collection, /**< A collection clip. @see CollectionClip. */
        video,      /**< A video clip. N.B. not yet imlemented. */
        recording,  /**< A temporary recording clip. */
        chord,      /**< A chord clip. @see ChordClip. */
        arranger    /**< An arranger clip. @see ArrangerClip. */
    };

    /** Creates a TrackItem with an ID and type.
        IDs should be unique within an Edit.
    */
    TrackItem (Edit&, EditItemID, Type);
    
    /** Destructor. */
    ~TrackItem();

    //==============================================================================
    /** Returns the string version of a TrackItem::Type. */
    static const char* typeToString (Type);

    /** Returns an Identifier version of a TrackItem::Type. */
    static juce::Identifier clipTypeToXMLType (Type);

    /** Returns the TrackItem::Type of a type string. */
    static TrackItem::Type xmlTagToType (juce::StringRef);

    /** Returns the TrackItem::Type of a type string. */
    static TrackItem::Type stringToType (const juce::String&);

    /** Returns a text string for a new clip of the given type.
        E.g. "New Audio Clip" for Type::wave
    */
    static juce::String getSuggestedNameForNewItem (Type);

    //==============================================================================
    /** Must return the track this item lives on. */
    virtual Track* getTrack() const = 0;

    /** Should return true if this clip is part of a group. */
    virtual bool isGrouped() const                      { return false; }

    /** If this clip is part of a group, this should return the parent item it belongs to. */
    virtual TrackItem* getGroupParent() const           { return {}; }

    //==============================================================================
    /** Must return the position of this item. */
    virtual ClipPosition getPosition() const = 0;

    /** Returns the time range of this item. */
    TimeRange getEditTimeRange() const                  { return getPosition().time; }

    /** Returns the start beat in the Edit of this item. */
    BeatPosition getStartBeat() const;

    /** Returns the start beat of the content in the Edit of this item.
        I.e. the beat index of (start - offset)
    */
    BeatPosition getContentStartBeat() const;

    /** Returns the end beat in the Edit of this item. */
    BeatPosition getEndBeat() const;

    /** Returns the duration in beats the of this item. */
    BeatDuration getLengthInBeats() const;

    /** Returns an Edit time point for a given number of beats from the start of this item. */
    TimePosition getTimeOfRelativeBeat (BeatDuration) const;

    /** Returns an Edit beat point for a given number of seconds from the start of this item. */
    BeatPosition getBeatOfRelativeTime (TimeDuration) const;

    /** Returns an the offset of this item in beats. */
    BeatDuration getOffsetInBeats() const;

    //==============================================================================
    /** Returns the ID of the Track this item lives on. */
    EditItemID getTrackID() const;

    const Type type; /**< The type of this item. */

    //==============================================================================
    /** Helper function to sort an array of TrackItem[s] by their start time. */
    template <typename ArrayType>
    static void sortByTime (ArrayType& items)
    {
        std::sort (items.begin(), items.end(),
                   [] (const TrackItem* a, const TrackItem* b) { return a->getPosition().time.getStart() < b->getPosition().time.getStart(); });
    }

    /** Helper function to sort an array of TrackItem[s] by their start time without
        changing the order of items at the same time.
    */
    template <typename ArrayType>
    static void stableSortByTime (ArrayType& items)
    {
        std::stable_sort (items.begin(), items.end(),
                          [] (const TrackItem* a, const TrackItem* b) { return a->getPosition().time.getStart() < b->getPosition().time.getStart(); });
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackItem)
};


}} // namespace tracktion { inline namespace engine
