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
*/
class TrackCompManager
{
public:
    TrackCompManager (Edit&);
    ~TrackCompManager();

    void initialise (const juce::ValueTree&);
    int getNumGroups() const;

    juce::StringArray getCompNames() const;
    juce::String getCompName (int index);
    void setCompName (int index, const juce::String& name);

    int addGroup (const juce::String& name);
    void removeGroup (int index);

    juce::Array<Track*> getTracksInComp (int index);

    static AudioNode* createTrackCompAudioNode (AudioNode* input,
                                                const juce::Array<EditTimeRange>& muteTimes,
                                                const juce::Array<EditTimeRange>& nonMuteTimes,
                                                double crossfadeTime);

    //==============================================================================
    struct CompSection  : public juce::ReferenceCountedObject
    {
        CompSection (const juce::ValueTree&);
        ~CompSection();

        using Ptr = juce::ReferenceCountedObjectPtr<CompSection>;
        static CompSection* createAndIncRefCount (const juce::ValueTree& v);

        void updateFrom (juce::ValueTree&, const juce::Identifier&);

        juce::ValueTree state;

        EditItemID getTrack() const      { return track; }
        double getEnd() const            { return end; }

    private:
        EditItemID track;
        double end;

        void updateTrack();
        void updateEnd();

        JUCE_DECLARE_WEAK_REFERENCEABLE (CompSection)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompSection)
    };

    //==============================================================================
    struct TrackComp    : public Selectable,
                          public juce::ReferenceCountedObject,
                          public ValueTreeObjectList<CompSection>
    {
        enum TimeFormat
        {
            seconds = 0,
            beats
        };

        TrackComp (Edit&, const juce::ValueTree&);
        ~TrackComp();

        using Ptr = juce::ReferenceCountedObjectPtr<TrackComp>;
        static TrackComp* createAndIncRefCount (Edit&, const juce::ValueTree&);

        juce::String getSelectableDescription() override;

        TimeFormat getTimeFormat() const noexcept       { return timeFormat; }

        void setTimeFormat (TimeFormat t);

        static juce::Array<EditTimeRange> getMuteTimes (const juce::Array<EditTimeRange>& nonMuteTimes);
        EditTimeRange getTimeRange() const;
        AudioNode* createAudioNode (Track&, AudioNode* input);

        void setName (const juce::String&);

        static EditItemID getSourceTrackID (const juce::ValueTree&);
        static void setSourceTrackID (juce::ValueTree&, EditItemID, juce::UndoManager*);

        void setSectionTrack (CompSection&, const Track::Ptr&);
        void removeSection (CompSection&);

        CompSection* moveSectionToEndAt (CompSection*, double newEndTime);
        CompSection* moveSection (CompSection*, double timeDelta);
        CompSection* moveSectionEndTime (CompSection*, double newTime);

        int removeSectionsWithinRange (EditTimeRange timeRange, CompSection* sectionToKeep);
        juce::ValueTree addSection (EditItemID trackID, double endTime, juce::UndoManager*);
        CompSection* addSection (Track::Ptr, double endTime);
        CompSection* splitSectionAtTime (double time);

        CompSection* findSectionWithEdgeTimeWithin (const Track::Ptr&, EditTimeRange timeRange, bool& startEdge) const;
        CompSection* findSectionAtTime (const Track::Ptr&, double time) const;

        struct Section
        {
            CompSection* compSection;
            EditTimeRange timeRange;
        };

        juce::Array<Section> getSectionsForTrack (const Track::Ptr&) const;
        bool isSuitableType (const juce::ValueTree&) const override;
        CompSection* createNewObject (const juce::ValueTree&) override;
        void deleteObject (CompSection*) override;
        void newObjectAdded (CompSection*) override;
        void objectRemoved (CompSection*) override;
        void objectOrderChanged() override;
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

        juce::ValueTree state;
        juce::CachedValue<juce::String> name;
        juce::CachedValue<juce::Colour> includedColour, excludedColour;

    private:
        Edit& edit;
        juce::CachedValue<TimeFormat> timeFormat;

        CompSection* getCompSectionFor (const juce::ValueTree&);
        void convertFromSecondsToBeats();
        void convertFromBeatsToSeconds();
        void convertTimes (TimeFormat, TimeFormat);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComp)
    };

    TrackComp::Ptr getTrackComp (AudioTrack*);

private:
    struct TrackCompList;

    Edit& edit;
    juce::ValueTree state;
    juce::ScopedPointer<TrackCompList> trackCompList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackCompManager)
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::TrackCompManager::TrackComp::TimeFormat>
    {
        static tracktion_engine::TrackCompManager::TrackComp::TimeFormat fromVar (const var& v)   { return (tracktion_engine::TrackCompManager::TrackComp::TimeFormat) static_cast<int> (v); }
        static var toVar (tracktion_engine::TrackCompManager::TrackComp::TimeFormat v)            { return static_cast<int> (v); }
    };
}
