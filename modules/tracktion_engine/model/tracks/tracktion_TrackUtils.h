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
    Defines the place to insert Track[s].
    @see Edit::insertNewTrack, Edit::moveTrack
*/
struct TrackInsertPoint
{
    /** Creates an insertion point with a parent and preceeding track.
        @param parent       The parent tracks should be nested in.
                            nullptr means a top-level track
        @param preceding    The track before the insertion point.
                            nullptr means tracks should be inserted at the start of the list.
    */
    TrackInsertPoint (Track* parent, Track* preceding);

    /** Creates an insertion point with a parent and preceeding track.
        @param parentTrackID    The ID of the parent tracks should be nested in.
                                An invalid ID means a top-level track
        @param precedingTrackID The ID of the track before the insertion point.
                                An invalid ID means tracks should be inserted at the start of the list.
    */
    TrackInsertPoint (EditItemID parentTrackID, EditItemID precedingTrackID);

    /** Creates an insertion point around a Track.
        @param currentPos   The track to base insertion around.
        @param insertBefore Whether new tracks should go before or after the currentPos.
    */
    TrackInsertPoint (Track& currentPos, bool insertBefore);

    /** Creates an insertion point after a given Track state. */
    TrackInsertPoint (const juce::ValueTree&);

    EditItemID parentTrackID, precedingTrackID;
};

//==============================================================================
/**
    An iterable list of Track[s] that live either in an Edit or as subtracks of a Track.
    @see Edit::getTrackList, Track::getSubTrackList
*/
struct TrackList    : public ValueTreeObjectList<Track>,
                      private juce::AsyncUpdater
{
    /** Creates a TrackList for a parent state. */
    TrackList (Edit&, const juce::ValueTree& parent);

    /** Destructor. */
    ~TrackList() override;

    //==============================================================================
    /** Returns a Track for a given state. */
    Track* getTrackFor (const juce::ValueTree&) const;

    /** Calls the given function on all Track[s].
        Return false from the function to stop the traversal.
        @returns true if all tracks were visited, false otherwise
    */
    bool visitAllRecursive (const std::function<bool(Track&)>&) const;

    /** Calls the given function on all top-level Track[s].
        Return false from the function to stop the traversal.
        @returns true if all tracks were visited, false otherwise
    */
    void visitAllTopLevel (const std::function<bool(Track&)>&) const;

    /** Calls the given function on all Track[s], optionally recursively.
        Return false from the function to stop the traversal.
        @param recursive Whether nested tracks should be visited
        @returns true if all tracks were visited, false otherwise
    */
    void visitAllTracks (const std::function<bool(Track&)>&, bool recursive) const;

    //==============================================================================
    /** Returns true if the track is movable. I.e. not a global track. */
    static bool isMovableTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the state is for an ArrangerTrack. */
    static bool isArrangerTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the state is for a ChordTrack. */
    static bool isChordTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the state is for a MarkerTrack. */
    static bool isMarkerTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the state is for a TempoTrack. */
    static bool isTempoTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the state is for a MasterTrack. */
    static bool isMasterTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the track is fixed. I.e. a global track.
        @see TempoTrack, MarkerTrack, ChordTrack, ArrangerTrack
    */
    static bool isFixedTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the given ValeTree is for a known Track type. */
    static bool isTrack (const juce::ValueTree&) noexcept;

    /** Returns true if the given Identifier is for a known Track type. */
    static bool isTrack (const juce::Identifier&) noexcept;

    /** Returns true if the track has any sub tracks.
        @see FolderTrack, AutomationTrack
    */
    static bool hasAnySubTracks (const juce::ValueTree&);

    //==============================================================================
    /** Sorts a list of tracks by their type, placing global tracks at the top. */
    static void sortTracksByType (juce::ValueTree& editState, juce::UndoManager*);

    //==============================================================================
    /** @internal */
    bool isSuitableType (const juce::ValueTree&) const override;
    /** @internal */
    Track* createNewObject (const juce::ValueTree&) override;
    /** @internal */
    void deleteObject (Track* t) override;
    /** @internal */
    void newObjectAdded (Track* t) override;
    /** @internal */
    void objectRemoved (Track*) override;
    /** @internal */
    void objectOrderChanged() override;

    Edit& edit;
    bool rebuilding = true;

private:
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackList)
};

//==============================================================================
/**
    Defines a time raneg sectin of a Track.
*/
struct TrackSection
{
    Track* track = nullptr; /**< The Track this section refers to. */
    TimeRange range;        /**< The time range this section refers to. */

    /** Merges an overlapping TrackItem track/time range with this section.
        @returns true if this item was from the same track and overlapping time
                 range and could be merged. false if it didn't intersect and should
                 be its own section.
    */
    bool merge (const TrackItem& c)
    {
        if (c.getTrack() == track
             && c.getEditTimeRange().overlaps (range.expanded (TimeDuration::fromSeconds (0.0001))))
        {
            range = range.getUnionWith (c.getEditTimeRange());
            return true;
        }

        return false;
    }

    /** Returns a set of TrackSections for the given TrackItems. */
    template <typename TrackItemArray>
    static juce::Array<TrackSection> findSections (const TrackItemArray& trackItems)
    {
        juce::Array<TrackSection> result;

        for (auto&& c : trackItems)
        {
            bool segFound = false;

            for (auto& dstSeg : result)
            {
                if (dstSeg.merge (*c))
                {
                    segFound = true;
                    break;
                }
            }

            if (! segFound)
            {
                TrackSection cs;
                cs.range = c->getEditTimeRange();
                cs.track = c->getTrack();
                
                if (cs.track != nullptr)
                    result.add (cs);
            }
        }

        return result;
    }
};

//==============================================================================
/**
    Holds a reference to a section of automation for a given Track.
*/
struct TrackAutomationSection
{
    /** Construts an empty section. */
    TrackAutomationSection() noexcept = default;

    /** Construts a section for a given TrackItem. */
    TrackAutomationSection (TrackItem&);

    TimeRange position;     /** The time range of the automation section. */
    Track::Ptr src,         /** The source Track. */
               dst;         /** The destination Track. */

    /** Merges another TrackAutomationSection with this one. */
    void mergeIn (const TrackAutomationSection&);

    /** Tests whether another section overlaps with this one. */
    bool overlaps (const TrackAutomationSection&) const;

    /** Tests whether this section contains a given parameter. */
    bool containsParameter (AutomatableParameter*) const;

    /** Holds a parameter and curve section. */
    struct ActiveParameters
    {
        AutomatableParameter::Ptr param;    /**< The parameter. */
        AutomationCurve curve;              /**< The curve section of this parameter. */
    };

    juce::Array<ActiveParameters> activeParameters; /**< A list of parameteres and their curves. */
};

/** Moves a set of automation optionally applying an offset and copying the
    automation (rather than moving it).
*/
void moveAutomation (const juce::Array<TrackAutomationSection>&, TimeDuration offset, bool copy);


//==============================================================================
/**
    Returns the index of the next item after the given time.
*/
template <typename ArrayType>
int findIndexOfNextItemAt (const ArrayType& items, TimePosition time)
{
    for (int i = items.size(); --i >= 0;)
    {
        auto pos = items.getUnchecked (i)->getPosition().time;

        if (pos.getStart() < time)
        {
            if (pos.getEnd() > time)
                return i;

            return i + 1;
        }
    }

    return 0;
}

/**
    Returns the the time range that covers all the given TrackItems.
*/
template <typename ArrayType>
TimeRange findUnionOfEditTimeRanges (const ArrayType& items)
{
    TimeRange total;
    bool first = true;

    for (auto& item : items)
    {
        auto time = item->getEditTimeRange();

        if (first)
        {
            first = false;
            total = time;
        }
        else
        {
            total = total.getUnionWith (time);
        }
    }

    return total;
}

}} // namespace tracktion { inline namespace engine
