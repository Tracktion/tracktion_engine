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

//==============================================================================
/**
    Base class for EditItems that live in a track, e.g. clips
*/
class TrackItem   : public EditItem,
                    public Selectable,
                    public juce::ReferenceCountedObject
{
public:
    enum class Type
    {
        unknown,
        wave,
        midi,
        edit,
        step,
        marker,
        pitch,
        timeSig,
        collection,
        video,
        recording,
        chord
    };

    TrackItem (Edit&, EditItemID, Type);
    ~TrackItem();

    static const char* typeToString (Type);
    static juce::Identifier clipTypeToXMLType (Type);
    static TrackItem::Type xmlTagToType (juce::StringRef);
    static TrackItem::Type stringToType (const juce::String&);
    static juce::String getSuggestedNameForNewItem (Type);

    virtual Track* getTrack() const = 0;
    virtual bool isGrouped() const                      { return false; }
    virtual TrackItem* getGroupParent() const           { return {}; }

    virtual ClipPosition getPosition() const = 0;
    EditTimeRange getEditTimeRange() const              { return getPosition().time; }

    double getStartBeat() const;
    double getContentStartBeat() const;            // the beat index of (start - offset)
    double getEndBeat() const;
    double getLengthInBeats() const;
    double getTimeOfRelativeBeat (double beat) const;
    double getBeatOfRelativeTime (double t) const;
    double getOffsetInBeats() const;

    EditItemID getTrackID() const;

    const Type type;

    template <typename ArrayType>
    static void sortByTime (ArrayType& items)
    {
        std::sort (items.begin(), items.end(),
                   [] (const TrackItem* a, const TrackItem* b) { return a->getPosition().time.start < b->getPosition().time.start; });
    }

    template <typename ArrayType>
    static void stableSortByTime (ArrayType& items)
    {
        std::stable_sort (items.begin(), items.end(),
                          [] (const TrackItem* a, const TrackItem* b) { return a->getPosition().time.start < b->getPosition().time.start; });
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackItem)
};


} // namespace tracktion_engine
