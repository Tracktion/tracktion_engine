/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class ClipTrack   : public Track
{
public:
    ClipTrack (Edit&, const juce::ValueTree&, double defaultHeight, double minHeight, double maxHeight);
    ~ClipTrack();

    void flushStateToValueTree() override;

    //==============================================================================
    const juce::Array<Clip*>& getClips() const noexcept;
    Clip* findClipForID (EditItemID) const override;

    //==============================================================================
    void refreshCollectionClips (Clip& newClip);
    CollectionClip* getCollectionClip (int index) const noexcept;
    CollectionClip* getCollectionClip (Clip*) const;
    int getNumCollectionClips() const noexcept;
    int indexOfCollectionClip (CollectionClip*) const;
    int getIndexOfNextCollectionClipAt (double time);
    CollectionClip* getNextCollectionClipAt (double time);
    bool contains (CollectionClip*) const;
    void addCollectionClip (CollectionClip*);
    void removeCollectionClip (CollectionClip*);

    //==============================================================================
    int getNumTrackItems() const override;
    TrackItem* getTrackItem (int idx) const override;
    int indexOfTrackItem (TrackItem*) const override;
    int getIndexOfNextTrackItemAt (double time) override;
    TrackItem* getNextTrackItemAt (double time) override;

    /** inserts space and moves everything up */
    void insertSpaceIntoTrack (double time, double amountOfSpace) override;

    //==============================================================================
    double getLength() const;
    double getLengthIncludingInputTracks() const;
    EditTimeRange getTotalRange() const;

    //==============================================================================
    void addClip (const Clip::Ptr& clip);

    Clip* insertClipWithState (juce::ValueTree);

    Clip* insertClipWithState (const juce::ValueTree& stateToUse,
                               const juce::String& name, TrackItem::Type type,
                               ClipPosition position,
                               bool deleteExistingClips,
                               bool allowSpottingAdjustment);

    Clip* insertNewClip (TrackItem::Type, EditTimeRange position, SelectionManager* selectionManagerToSelectWith);
    Clip* insertNewClip (TrackItem::Type, const juce::String& name, EditTimeRange position, SelectionManager* selectionManagerToSelectWith);
    Clip* insertNewClip (TrackItem::Type, const juce::String& name, ClipPosition position, SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (const juce::String& name, const juce::File& sourceFile,
                                                                   ClipPosition position, bool deleteExistingClips);

    juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (const juce::String& name, ProjectItemID sourceID,
                                                                   ClipPosition position, bool deleteExistingClips);

    juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (EditTimeRange position,
                                                              SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (const juce::String& name, EditTimeRange position,
                                                              SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<EditClip> insertEditClip (EditTimeRange position, ProjectItemID sourceID);

    void deleteRegion (EditTimeRange range, SelectionManager*);
    void deleteRegionOfClip (Clip::Ptr, EditTimeRange range, SelectionManager*);

    /** breaks a clip into 2 bits */
    Clip* splitClip (Clip&, double time);

    /** split all clips at this time */
    void splitAt (double time);

    /** finds the next cut point */
    double getNextTimeOfInterest (double afterThisTime);
    double getPreviousTimeOfInterest (double beforeThisTime);

    bool containsPlugin (const Plugin*) const override;
    Plugin::Array getAllPlugins() const override;
    void sendMirrorUpdateToAllPlugins (Plugin&) const override;

    bool areAnyClipsUsingFile (const AudioFile&);
    bool containsAnyMIDIClips() const;

protected:
    friend class Clip;

    juce::Array<double> findAllTimesOfInterest();

    struct ClipList;
    friend struct ClipList;
    juce::ScopedPointer<ClipList> clipList;

    struct CollectionClipList;
    friend struct CollectionClipList;
    juce::ScopedPointer<CollectionClipList> collectionClipList;

    void refreshTrackItems() const;

    mutable bool trackItemsDirty = false;
    mutable juce::Array<TrackItem*> trackItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipTrack)
};

} // namespace tracktion_engine
