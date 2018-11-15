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
class FolderTrack  : public Track
{
public:
    FolderTrack (Edit&, const juce::ValueTree&);
    ~FolderTrack();

    using Ptr = juce::ReferenceCountedObjectPtr<FolderTrack>;

    //==============================================================================
    juce::String getSelectableDescription() override;
    bool isFolderTrack() const override;

    void initialise() override;

    void sanityCheckName() override;
    juce::String getName() override;

    // not an index - values start from 1
    int getFolderTrackNumber() noexcept;

    //==============================================================================
    bool isSubmixFolder() const;
    bool outputsToDevice (const OutputDevice&) const;

    AudioNode* createAudioNode (const CreateAudioNodeParams&);

    //==============================================================================
    bool isFrozen (FreezeType) const override;

    //==============================================================================
    float getVcaDb (double tm);
    VCAPlugin* getVCAPlugin();
    VolumeAndPanPlugin* getVolumePlugin();

    //==============================================================================
    void generateCollectionClips (SelectionManager&);
    CollectionClip* getCollectionClip (int index)  const noexcept;
    int getNumCollectionClips() const noexcept;
    int indexOfCollectionClip (CollectionClip*) const;
    int getIndexOfNextCollectionClipAt (double time);
    CollectionClip* getNextCollectionClipAt (double time);
    bool contains (CollectionClip*) const;

    //==============================================================================
    int getNumTrackItems() const override;
    TrackItem* getTrackItem (int idx) const override;
    int indexOfTrackItem (TrackItem*) const override;
    int getIndexOfNextTrackItemAt (double time) override;
    TrackItem* getNextTrackItemAt (double time) override;

    //==============================================================================
    bool isMuted (bool includeMutingByDestination) const override;
    bool isSolo (bool includeIndirectSolo) const override;
    bool isSoloIsolate (bool includeIndirectSolo) const override;

    void setMute (bool) override;
    void setSolo (bool) override;
    void setSoloIsolate (bool) override;

    //==============================================================================
    void setDirtyClips();

    bool canContainPlugin (Plugin*) const override;
    bool willAcceptPlugin (Plugin&);

private:
    friend class Edit;

    juce::ReferenceCountedArray<CollectionClip> collectionClips;
    juce::CachedValue<bool> muted, soloed, soloIsolated;
    bool dirtyClips = true;

    juce::ReferenceCountedObjectPtr<VCAPlugin> vcaPlugin;
    juce::ReferenceCountedObjectPtr<VolumeAndPanPlugin> volumePlugin;
    AsyncCaller pluginUpdater;

    void updatePlugins();
    EditTimeRange getClipExtendedBounds (Clip&);

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
};

} // namespace tracktion_engine
