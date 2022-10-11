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

/** */
class ClipTrack   : public Track
{
public:
    ClipTrack (Edit&, const juce::ValueTree&, double defaultHeight, double minHeight, double maxHeight);
    ~ClipTrack() override;

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
    int getIndexOfNextCollectionClipAt (TimePosition);
    CollectionClip* getNextCollectionClipAt (TimePosition);
    bool contains (CollectionClip*) const;
    void addCollectionClip (CollectionClip*);
    void removeCollectionClip (CollectionClip*);

    //==============================================================================
    int getNumTrackItems() const override;
    TrackItem* getTrackItem (int idx) const override;
    int indexOfTrackItem (TrackItem*) const override;
    int getIndexOfNextTrackItemAt (TimePosition) override;
    TrackItem* getNextTrackItemAt (TimePosition) override;

    /** inserts space and moves everything up */
    void insertSpaceIntoTrack (TimePosition, TimeDuration) override;

    //==============================================================================
    TimeDuration getLength() const;
    TimeDuration getLengthIncludingInputTracks() const;
    TimeRange getTotalRange() const;

    //==============================================================================
    bool addClip (const Clip::Ptr& clip);

    Clip* insertClipWithState (juce::ValueTree);

    Clip* insertClipWithState (const juce::ValueTree& stateToUse,
                               const juce::String& name, TrackItem::Type type,
                               ClipPosition position,
                               bool deleteExistingClips,
                               bool allowSpottingAdjustment);

    Clip* insertNewClip (TrackItem::Type, TimeRange position, SelectionManager* selectionManagerToSelectWith);
    Clip* insertNewClip (TrackItem::Type, const juce::String& name, TimeRange position, SelectionManager* selectionManagerToSelectWith);
    Clip* insertNewClip (TrackItem::Type, const juce::String& name, ClipPosition position, SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (const juce::String& name, const juce::File& sourceFile,
                                                                   ClipPosition position, bool deleteExistingClips);

    juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (const juce::String& name, ProjectItemID sourceID,
                                                                   ClipPosition position, bool deleteExistingClips);

    juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (TimeRange position,
                                                              SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<MidiClip> insertMIDIClip (const juce::String& name, TimeRange position,
                                                              SelectionManager* selectionManagerToSelectWith);

    juce::ReferenceCountedObjectPtr<EditClip> insertEditClip (TimeRange position, ProjectItemID sourceID);

    void deleteRegion (TimeRange, SelectionManager*);
    void deleteRegionOfClip (Clip::Ptr, TimeRange, SelectionManager*);

    /** breaks a clip into 2 bits */
    Clip* splitClip (Clip&, TimePosition);

    /** split all clips at this time */
    void splitAt (TimePosition);

    /** finds the next cut point */
    TimePosition getNextTimeOfInterest (TimePosition afterThisTime);
    TimePosition getPreviousTimeOfInterest (TimePosition beforeThisTime);

    bool containsPlugin (const Plugin*) const override;
    Plugin::Array getAllPlugins() const override;
    void sendMirrorUpdateToAllPlugins (Plugin&) const override;

    bool areAnyClipsUsingFile (const AudioFile&);
    bool containsAnyMIDIClips() const;

protected:
    friend class Clip;

    juce::Array<TimePosition> findAllTimesOfInterest();

    struct ClipList;
    friend struct ClipList;
    std::unique_ptr<ClipList> clipList;

    struct CollectionClipList;
    friend struct CollectionClipList;
    std::unique_ptr<CollectionClipList> collectionClipList;

    void refreshTrackItems() const;

    mutable bool trackItemsDirty = false;
    mutable juce::Array<TrackItem*> trackItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipTrack)
};

}} // namespace tracktion { inline namespace engine
