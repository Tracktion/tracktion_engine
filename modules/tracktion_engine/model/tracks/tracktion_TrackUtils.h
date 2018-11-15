/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct TrackInsertPoint
{
    TrackInsertPoint (Track* parent, Track* preceding);
    TrackInsertPoint (EditItemID parentTrackID, EditItemID precedingTrackID);
    TrackInsertPoint (Track& currentPos, bool insertBefore);
    TrackInsertPoint (const juce::ValueTree&);

    EditItemID parentTrackID, precedingTrackID;
};

//==============================================================================
struct TrackList    : public ValueTreeObjectList<Track>,
                      private juce::AsyncUpdater
{
    TrackList (Edit& e, const juce::ValueTree& parent);
    ~TrackList();

    Track* getTrackFor (const juce::ValueTree&) const;

    bool visitAllRecursive (const std::function<bool(Track&)>& f) const;
    void visitAllTopLevel (const std::function<bool(Track&)>& f) const;
    void visitAllTracks (const std::function<bool(Track&)>& f, bool recursive) const;

    static bool isMovableTrack (const juce::ValueTree&) noexcept;
    static bool isChordTrack (const juce::ValueTree&) noexcept;
    static bool isMarkerTrack (const juce::ValueTree&) noexcept;
    static bool isTempoTrack (const juce::ValueTree&) noexcept;
    static bool isFixedTrack (const juce::ValueTree&) noexcept;
    static bool isTrack (const juce::ValueTree&) noexcept;
    static bool isTrack (const juce::Identifier&) noexcept;
    static bool hasAnySubTracks (const juce::ValueTree&);

    static void sortTracksByType (juce::ValueTree& editState, juce::UndoManager*);

    bool isSuitableType (const juce::ValueTree&) const override;
    Track* createNewObject (const juce::ValueTree&) override;
    void deleteObject (Track* t) override;
    void newObjectAdded (Track* t) override;
    void objectRemoved (Track*) override;
    void objectOrderChanged() override;

    Edit& edit;
    bool rebuilding = true;

private:
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackList)
};

//==============================================================================
struct TrackSection
{
    Track* track;
    EditTimeRange range;

    bool merge (const TrackItem& c)
    {
        if (c.getTrack() == track
             && c.getEditTimeRange().overlaps (range.expanded (0.0001)))
        {
            range = range.getUnionWith (c.getEditTimeRange());
            return true;
        }

        return false;
    }

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
                result.add (cs);
            }
        }

        return result;
    }
};

//==============================================================================
struct TrackAutomationSection
{
    TrackAutomationSection() noexcept {}
    TrackAutomationSection (TrackItem&);

    EditTimeRange position;
    Track::Ptr src, dst;

    void mergeIn (const TrackAutomationSection&);
    bool overlaps (const TrackAutomationSection&) const;
    bool containsParameter (AutomatableParameter*) const;

    struct ActiveParameters
    {
        AutomatableParameter::Ptr param;
        AutomationCurve curve;
    };

    juce::Array<ActiveParameters> activeParameters;
};

void moveAutomation (const juce::Array<TrackAutomationSection>&, double offset, bool copy);

//==============================================================================
bool checkRenderParametersAndConfirmWithUser (const juce::Array<Track*>&,
                                              EditTimeRange markedRange,
                                              bool silent = false);

//==============================================================================
template <typename ArrayType>
int findIndexOfNextItemAt (const ArrayType& items, double time)
{
    for (int i = items.size(); --i >= 0;)
    {
        auto pos = items.getUnchecked(i)->getPosition().time;

        if (pos.getStart() < time)
        {
            if (pos.getEnd() > time)
                return i;

            return i + 1;
        }
    }

    return 0;
}

template <typename ArrayType>
EditTimeRange findUnionOfEditTimeRanges (const ArrayType& items)
{
    EditTimeRange total;
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

} // namespace tracktion_engine
