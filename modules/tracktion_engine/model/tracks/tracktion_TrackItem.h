/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
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

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackItem)
};

//==============================================================================
struct TrackItemStartTimeSorter
{
    static int compareElements (const TrackItem* first, const TrackItem* second) noexcept
    {
        auto t1 = first->getPosition().time.start;
        auto t2 = second->getPosition().time.start;
        return t1 < t2 ? -1 : (t2 < t1 ? 1 : 0);
    }
};


} // namespace tracktion_engine
