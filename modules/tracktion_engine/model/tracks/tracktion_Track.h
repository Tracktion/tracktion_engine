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
class Track   : public EditItem,
                public Selectable,
                public juce::ReferenceCountedObject,
                public MacroParameterElement,
                protected juce::ValueTree::Listener
{
public:
    Track (Edit&, const juce::ValueTree&,
           double defaultTrackHeight, double minTrackHeight, double maxTrackHeight);

    ~Track();

    using Ptr   = juce::ReferenceCountedObjectPtr<Track>;
    using Array = juce::ReferenceCountedArray<Track>;

    //==============================================================================
    virtual void initialise();
    virtual void flushStateToValueTree();
    void refreshCurrentAutoParam();

    //==============================================================================
    juce::String getName() override                             { return trackName; }
    void setName (const juce::String&);
    void resetName();
    virtual void sanityCheckName()                              {}

    virtual bool isAudioTrack() const                           { return false; }
    virtual bool isAutomationTrack() const                      { return false; }
    virtual bool isFolderTrack() const                          { return false; }
    virtual bool isMarkerTrack() const                          { return false; }
    virtual bool isTempoTrack() const                           { return false; }
    virtual bool isChordTrack() const                           { return false; }

    bool canContainMarkers() const                              { return isMarkerTrack(); }
    bool canContainMIDI() const                                 { return isAudioTrack(); }
    bool canContainAudio() const                                { return isAudioTrack(); }
    bool canContainEditClips() const                            { return isAudioTrack(); }

    bool canContainPlugins() const                              { return isAudioTrack() || isFolderTrack(); }
    bool isMovable() const                                      { return isAudioTrack() || isFolderTrack(); }
    bool isOnTop() const                                        { return isMarkerTrack() || isTempoTrack() || isChordTrack(); }
    bool acceptsInput() const                                   { return isAudioTrack(); }
    bool createsOutput() const                                  { return isAudioTrack(); }
    bool wantsAutomation() const                                { return ! (isMarkerTrack() || isChordTrack());  }

    virtual bool canContainPlugin (Plugin*) const = 0;

    enum FreezeType
    {
        groupFreeze = 0,
        individualFreeze,
        anyFreeze
    };

    virtual bool isFrozen (FreezeType) const                    { return false; }
    virtual void setFrozen (bool, FreezeType)                   {}

    bool isHidden() const                                       { return hidden; }
    void setHidden (bool h)                                     { hidden = h; }

    bool isProcessing (bool includeParents) const;
    void setProcessing (bool p)                                 { processing = p; }

    virtual bool processAudioNodesWhileMuted() const            { return false; }

    //==============================================================================
    juce::Array<Track*> getAllSubTracks (bool recursive) const;
    juce::Array<AudioTrack*> getAllAudioSubTracks (bool recursive) const;

    TrackList* getSubTrackList() const                          { return trackList; }
    bool hasSubTracks() const                                   { return trackList != nullptr; }
    virtual Clip* findClipForID (EditItemID) const;

    Track* getSiblingTrack (int delta, bool keepWithinSameParent) const;

    virtual int getNumTrackItems() const                        { return 0; }
    virtual TrackItem* getTrackItem (int /*index*/) const       { return {}; }
    virtual int indexOfTrackItem (TrackItem*) const             { return -1; }
    virtual int getIndexOfNextTrackItemAt (double /*time*/)     { return -1; }
    virtual TrackItem* getNextTrackItemAt (double /*time*/)     { return {}; }

    virtual void insertSpaceIntoTrack (double time, double amountOfSpace);

    //==============================================================================
    juce::ValueTree getParentTrackTree() const;
    Track* getParentTrack() const                               { return cachedParentTrack.get(); }
    FolderTrack* getParentFolderTrack() const                   { return getParentTrack() != nullptr ? cachedParentFolderTrack : nullptr; }
    bool isAChildOf (const Track&) const;
    bool isPartOfSubmix() const;

    int getIndexInEditTrackList() const;
    int getTrackDepth() const;  // number of parents within which track is nested

    //==============================================================================
    virtual bool isMuted (bool /*includeMutingByDestination*/) const    { return false; }
    virtual bool isSolo (bool /*includeIndirectSolo*/) const            { return false; }
    virtual bool isSoloIsolate (bool /*includeIndirectSolo*/) const     { return false; }

    virtual void setMute (bool)                                         {}
    virtual void setSolo (bool)                                         {}
    virtual void setSoloIsolate (bool)                                  {}

    enum MuteAndSoloLightState
    {
        soloLit         = 1,
        soloFlashing    = 2,
        soloIsolate     = 4,

        muteLit         = 8,
        muteFlashing    = 16
    };

    MuteAndSoloLightState getMuteAndSoloLightState() const;

    bool shouldBePlayed() const noexcept                                { return isAudible; }
    void updateAudibility (bool areAnyTracksSolo);

    //==============================================================================
    juce::Array<AutomatableParameter*> getAllAutomatableParams() const;

    AutomatableParameter* getCurrentlyShownAutoParam() const noexcept   { return currentAutoParam; }
    void setCurrentlyShownAutoParam (const AutomatableParameter::Ptr&);
    void hideAutomatableParametersForSource (EditItemID pluginOrParameterID);
    AutomatableParameter* chooseDefaultAutomationCurve() const;

    //==============================================================================
    /** Also true if the plugin's in any clips in this track as well  */
    virtual bool containsPlugin (const Plugin*) const;
    bool hasFreezePointPlugin() const;

    juce::Array<AutomatableEditItem*> getAllAutomatableEditItems() const;
    virtual Plugin::Array getAllPlugins() const;
    virtual void sendMirrorUpdateToAllPlugins (Plugin&) const;

    void flipAllPluginsEnablement();

    //==============================================================================
    ModifierList& getModifierList() const                   { return *modifierList; }

    //==============================================================================
    static const int minTrackHeightForDetail = 10;
    static const int trackHeightForEditor = 180;
    static const int frozenTrackHeight = 15;

    const double defaultTrackHeight, minTrackHeight, maxTrackHeight;

    //==============================================================================
    // user-specified colour:
    void setColour (juce::Colour newColour)                 { colour = newColour; }
    juce::Colour getColour() const                          { return colour; }

    bool canShowImage() const;
    void setTrackImage (const juce::String& idOrData);
    juce::String getTrackImage() const                      { return imageIdOrData; }
    bool imageHasChanged();

    void setTags (const juce::StringArray&);
    juce::String getTags() const                            { return tags.get(); }
    const juce::StringArray& getTagsArray() const noexcept  { return tagsArray; }

    //==============================================================================
    juce::ValueTree state;
    PluginList pluginList;

    juce::WeakReference<Track>::Master masterReference;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override;

    virtual bool isTrackAudible (bool areAnyTracksSolo) const;

private:
    AutomatableParameter* currentAutoParam = nullptr;
    juce::ScopedPointer<TrackList> trackList;
    std::unique_ptr<ModifierList> modifierList;

    juce::CachedValue<juce::String> trackName;
    juce::CachedValue<bool> hidden, processing;

    juce::CachedValue<EditItemID> currentAutoParamPlugin;
    juce::CachedValue<juce::String> currentAutoParamID;

    juce::CachedValue<juce::Colour> colour;
    juce::CachedValue<juce::String> imageIdOrData, tags;
    juce::StringArray tagsArray;

    bool imageChanged = false;
    bool isAudible = true;

    juce::WeakReference<Track> cachedParentTrack;
    FolderTrack* cachedParentFolderTrack = nullptr;

    //==============================================================================
    void updateTrackList();
    void updateCachedParent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};

} // namespace tracktion_engine
