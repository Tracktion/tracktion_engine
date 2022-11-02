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
    Base class for tracks which contain clips and plugins and can be added to Edit[s].
    @see AudioTrack, FolderTrack, AutomationTrack, MarkerTrack, TempoTrack, ChordTrack, ArrangerTrack
*/
class Track   : public EditItem,
                public Selectable,
                public juce::ReferenceCountedObject,
                public MacroParameterElement,
                protected juce::ValueTree::Listener,
                protected juce::AsyncUpdater
{
public:
    /** Creates a track with a given state. */
    Track (Edit&, const juce::ValueTree&,
           double defaultTrackHeight, double minTrackHeight, double maxTrackHeight);

    /** Destructor. */
    ~Track() override;

    using Ptr   = juce::ReferenceCountedObjectPtr<Track>;
    using Array = juce::ReferenceCountedArray<Track>;

    //==============================================================================
    /** Initialises the Track.
        You shouldn't need to call this directly, it will be called by the track
        creation methods.
        @see Edit::insertTrack
    */
    virtual void initialise();
    
    /** Flushes all plugin states on the track to the state object.
        This is usually called automatically when an Edit is saved.
    */
    virtual void flushStateToValueTree();
    
    /** Updates the current parameter bases on the set IDs.
        This is usually called automatically by the Edit when the ID changes or an
        Edit is loaded.
        @see getCurrentlyShownAutoParam
    */
    void refreshCurrentAutoParam();

    //==============================================================================
    /** Returns the name of the Track. */
    juce::String getName() override                             { return trackName; }

    /** Sets the name of the Track. */
    void setName (const juce::String&);
    
    /** Sets the name of the Track to an empty string.
        Base classes may then use this an an indication to return a name based on
        the track type and index
    */
    void resetName();
    
    /** Sub-classes can impliment this to avoid certain characters being used in a name. */
    virtual void sanityCheckName()                              {}

    //==============================================================================
    /** Returns true if this is an AudioTrack. */
    virtual bool isAudioTrack() const                           { return false; }

    /** Returns true if this is an AutomationTrack. */
    virtual bool isAutomationTrack() const                      { return false; }

    /** Returns true if this is a FolderTrack. */
    virtual bool isFolderTrack() const                          { return false; }

    /** Returns true if this is a MarkerTrack. */
    virtual bool isMarkerTrack() const                          { return false; }

    /** Returns true if this is a TempoTrack. */
    virtual bool isTempoTrack() const                           { return false; }

    /** Returns true if this is a ChordTrack. */
    virtual bool isChordTrack() const                           { return false; }

    /** Returns true if this is an ArrangerTrack. */
    virtual bool isArrangerTrack() const                        { return false; }

    /** Returns true if this is a MasterTrack. */
    virtual bool isMasterTrack() const                          { return false; }

    //==============================================================================
    /** Returns true if this Track can contain MarkerClip[s]. */
    bool canContainMarkers() const                              { return isMarkerTrack(); }

    /** Returns true if this Track can contain MidiClip[s]. */
    bool canContainMIDI() const                                 { return isAudioTrack(); }

    /** Returns true if this Track can contain WaveAudioClip[s]. */
    bool canContainAudio() const                                { return isAudioTrack(); }

    /** Returns true if this Track can contain EditClip[s]. */
    bool canContainEditClips() const                            { return isAudioTrack(); }

    /** Returns true if this Track can contain Plugin[s]. */
    bool canContainPlugins() const                              { return isAudioTrack() || isFolderTrack() || isMasterTrack(); }

    /** Returns true if this Track is movable. @see AudioTrack, FolderTrack */
    bool isMovable() const                                      { return isAudioTrack() || isFolderTrack(); }

    /** Returns true if this a global Track and should be on top of others. @see MarkerTrack, TempoTrack */
    bool isOnTop() const;

    /** Returns true if this track can have inputs assigned to it. @see AudioTrack */
    bool acceptsInput() const                                   { return isAudioTrack(); }

    /** Returns true if this track creates audible output. @see AudioTrack */
    bool createsOutput() const                                  { return isAudioTrack(); }

    /** Returns true if this track can show automation. @see AudioTrack, FolderTrack, AutomationTrack */
    bool wantsAutomation() const                                { return ! (isMarkerTrack() || isChordTrack() || isArrangerTrack());  }

    /** Returns true if this track can contain a specific Plugin.
        Subclasses can override this to avoid specific plugins such as VCAPlugin[s] on AudioTrack[s].
    */
    virtual bool canContainPlugin (Plugin*) const = 0;

    //==============================================================================
    /** Determines the type of freeze. */
    enum FreezeType
    {
        groupFreeze = 0,    /**< Freezes multiple tracks together in to a single file. */
        individualFreeze,   /**< Freezes a track in to a single audio file. */
        anyFreeze           /**< Either a group or individual freeze. */
    };

    /** Returns true if this track is frozen using the given type. */
    virtual bool isFrozen (FreezeType) const                    { return false; }

    /** Attempts to freeze or unfreeze the track using a given FreezeType. */
    virtual void setFrozen (bool /*shouldBeFrozen*/, FreezeType){}

    /** Returns true if this track should be hidden from view. */
    bool isHidden() const                                       { return hidden; }

    /** Sets whether this track should be hidden from view. */
    void setHidden (bool h)                                     { hidden = h; }

    /** Returns true if this track should be included in playback. */
    bool isProcessing (bool includeParents) const;

    /** Sets whether this track should be included in playback. */
    void setProcessing (bool p)                                 { processing = p; }

    /** Subclasses can override this to ensure track contents are still played even
        when the track is muted.
        This may be required if this track is a sidechain source or feeding an aux for example.
    */
    virtual bool processAudioNodesWhileMuted() const            { return false; }

    /** Should return any tracks which feed into this track. */
    virtual juce::Array<Track*> getInputTracks() const          { return {}; }

    //==============================================================================
    /** Returns all nested tracks.
        @param recursive Whether to include all tracks recursively.
    */
    juce::Array<Track*> getAllSubTracks (bool recursive) const;

    /** Returns all nested AudioTrack[s].
        @param recursive Whether to include all tracks recursively.
    */
    juce::Array<AudioTrack*> getAllAudioSubTracks (bool recursive) const;

    /** Returns the TrackList if this Track has any sub-tracks.
        May be nullptr if it doesn't.
    */
    TrackList* getSubTrackList() const                          { return trackList.get(); }
    
    /** Returns true if this track has any subtracks. */
    bool hasSubTracks() const                                   { return trackList != nullptr; }
    
    /** Returns a clip one with a matching ID can be found on this Track. */
    virtual Clip* findClipForID (EditItemID) const;

    /** Returns a sibling Track to this one.
        @param delta                The offset from this track. 1 is the next track,
                                    -1 is the previous.
        @param keepWithinSameParent Whether to include parent and sub tracks or only
                                    tracks at this level.
    */
    Track* getSiblingTrack (int delta, bool keepWithinSameParent) const;

    //==============================================================================
    /** Should return the number of TrackItem[s] on this Track. */
    virtual int getNumTrackItems() const                        { return 0; }

    /** Should return the TrackItem at the given index. */
    virtual TrackItem* getTrackItem (int /*index*/) const       { return {}; }

    /** Should return the index of the given TrackItem. */
    virtual int indexOfTrackItem (TrackItem*) const             { return -1; }

    /** Should return the index of the TrackItem after this time. */
    virtual int getIndexOfNextTrackItemAt (TimePosition)        { return -1; }

    /** Should return the TrackItem after this time. */
    virtual TrackItem* getNextTrackItemAt (TimePosition)        { return {}; }

    //==============================================================================
    /** Should insert empty space in to the track, shuffling down any items after the time.
        @param time             The time point in seconds to insert at
        @param amountOfSpace    The duration of time to insert
    */
    virtual void insertSpaceIntoTrack (TimePosition, TimeDuration);

    //==============================================================================
    /** Returns the state of the parent Track. */
    juce::ValueTree getParentTrackTree() const;

    /** Returns the parent Track if this is a nested track. */
    Track* getParentTrack() const                               { return dynamic_cast<Track*> (cachedParentTrack.get()); }

    /** Returns the parent FolderTrack if this is nested in one. */
    FolderTrack* getParentFolderTrack() const                   { return getParentTrack() != nullptr ? cachedParentFolderTrack : nullptr; }

    /** Tests whether this is a child of a given Track. */
    bool isAChildOf (const Track& possibleParent) const;

    /** Tests whether this nested within a submix FolderTrack.
        @see FolderTrack::isSubmixFolder
    */
    bool isPartOfSubmix() const;

    /** Returns the index of this track in a flat list of tracks contained in an Edit.
        Useful for naming Tracks.
    */
    int getIndexInEditTrackList() const;
    
    /** Returns the number of parents within which this track is nested */
    int getTrackDepth() const;

    //==============================================================================
    /** Returns true if this track is muted.
        @param includeMutingByDestination   If this is true, this will retrn true if any
                                            tracks this feeds in to are muted.
    */
    virtual bool isMuted (bool /*includeMutingByDestination*/) const    { return false; }

    /** Returns true if this track is soloed.
        @param includeIndirectSolo  If this is true, this will retrn true if any
                                    tracks this feeds in to are soloed.
    */
    virtual bool isSolo (bool /*includeIndirectSolo*/) const            { return false; }

    /** Returns true if this track is solo isolated.
        @param includeIndirectSolo  If this is true, this will retrn true if any
                                    tracks this feeds in to are solo isolated.
    */
    virtual bool isSoloIsolate (bool /*includeIndirectSolo*/) const     { return false; }

    /** Subclasses should implement this to mute themselves. */
    virtual void setMute (bool)                                         {}

    /** Subclasses should implement this to solo themselves. */
    virtual void setSolo (bool)                                         {}

    /** Subclasses should implement this to solo isolate themselves. */
    virtual void setSoloIsolate (bool)                                  {}

    /** Determines the status of the mute and solo indicators. */
    enum MuteAndSoloLightState
    {
        soloLit         = 1,    /**< Track is explicitly soloed. */
        soloFlashing    = 2,    /**< Track is implicitly soloed. */
        soloIsolate     = 4,    /**< Track is explicitly solo isolated. */

        muteLit         = 8,    /**< Track is explicitly muted. */
        muteFlashing    = 16    /**< Track is implicitly muted. */
    };

    /** Returns the mute a solo status. */
    MuteAndSoloLightState getMuteAndSoloLightState() const;

    /** Tests whether this track should be audible in the playback graph i.e.
        explicitly or implicitly soloed or not muted.
    */
    bool shouldBePlayed() const noexcept                                { return isAudible; }

    /** Updates the audibility state of the Track.
        N.B. You shouldn't need to call this directly. It will get called by the Edit
        when a track's solo/mute status changes.
    */
    void updateAudibility (bool areAnyTracksSolo);

    //==============================================================================
    /** Returns all the parameteres for this track's Plugin[s] and Modifier[s]. */
    juce::Array<AutomatableParameter*> getAllAutomatableParams() const;

    /** Returns the parameter whos curve should be shown on this Track. */
    AutomatableParameter* getCurrentlyShownAutoParam() const noexcept;

    /** Sets a parameter to display on this Track. */
    void setCurrentlyShownAutoParam (const AutomatableParameter::Ptr&);

    /** Hides a shown parameter if it matches the given ID. */
    void hideAutomatableParametersForSource (EditItemID pluginOrParameterID);

    //==============================================================================
    /** Tests whether this Track or a clip on it contains the given plugin.  */
    virtual bool containsPlugin (const Plugin*) const;

    /** Tests whether this Track contains a FreezePointPlugin.  */
    bool hasFreezePointPlugin() const;

    /** Returns all AutomatableEditItem[s] on this Track.
        @see Plugin, Modifier
    */
    juce::Array<AutomatableEditItem*> getAllAutomatableEditItems() const;
    
    /** Returns all pugins on this Track.
        Subclasses may implement this to also return Plugin[s] on Clip[s]
    */
    virtual Plugin::Array getAllPlugins() const;
    
    /** Sends a message to all plugins that the given plugin has changed. */
    virtual void sendMirrorUpdateToAllPlugins (Plugin& changedPlugin) const;

    /** Toggles the Plugin::isEnabled state for all Plugin[s] on this Track. */
    void flipAllPluginsEnablement();

    //==============================================================================
    /** Returns the ModifierList for this Track. */
    ModifierList& getModifierList() const                   { return *modifierList; }

    //==============================================================================
    static const int minTrackHeightForDetail = 10;  /**< The minimim height to show track contents at. */
    static const int trackHeightForEditor = 180;    /**< The height at which inline editors should be shown. */
    static const int frozenTrackHeight = 15;        /**< The height to show group frozen tracks. */

    const double defaultTrackHeight;    /**< The default height of this Track. */
    const double minTrackHeight;        /**< The minimum height of this Track. */
    const double maxTrackHeight;        /**< The maximum height of this Track. */

    //==============================================================================
    /** Sets a colour for this track to use. */
    void setColour (juce::Colour newColour)                 { colour = newColour; }

    /** Returns the the of this. */
    juce::Colour getColour() const                          { return colour; }

    //==============================================================================
    /** Tests whether this Track can show an image.
        N.B. May be removed in a future version.
    */
    bool canShowImage() const;

    /** Sets some image data to use.
        N.B. May be removed in a future version.
    */
    void setTrackImage (const juce::String& idOrData);

    /** Returns previously set image data.
        N.B. May be removed in a future version.
    */
    juce::String getTrackImage() const                      { return imageIdOrData; }

    /** Tests and resets a flag internally kept when the image changes.
        N.B. May be removed in a future version.
    */
    bool imageHasChanged();

    /** Sets an array of Strings to use as tags.
        N.B. May be removed in a future version.
    */
    void setTags (const juce::StringArray&);

    /** Returns the tags as a pipe-separated single String.
        N.B. May be removed in a future version.
    */
    juce::String getTags() const                            { return tags.get(); }

    /** Returns the array of tags.
        N.B. May be removed in a future version.
    */
    const juce::StringArray& getTagsArray() const noexcept  { return tagsArray; }

    //==============================================================================
    juce::ValueTree state;  /**< The state of this Track. */
    PluginList pluginList;  /**< The Track's PluginList. */

protected:
    /** @internal */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    /** @internal */
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    /** @internal */
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    /** @internal */
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    /** @internal */
    void valueTreeParentChanged (juce::ValueTree&) override;
    /** @internal */
    void handleAsyncUpdate() override;

    /** Returns whether this Track should be audible.
        Subclasses can override for custom behaviour.
    */
    virtual bool isTrackAudible (bool areAnyTracksSolo) const;

private:
    juce::WeakReference<Selectable> currentAutoParam;
    std::unique_ptr<TrackList> trackList;
    std::unique_ptr<ModifierList> modifierList;

    juce::CachedValue<juce::String> trackName;
    juce::CachedValue<bool> hidden, processing;

    juce::CachedValue<EditItemID> currentAutoParamPlugin;
    juce::CachedValue<juce::String> currentAutoParamID;

    juce::CachedValue<juce::Colour> colour;
    juce::CachedValue<juce::String> imageIdOrData, tags;
    juce::StringArray tagsArray;

    bool imageChanged = false;
    std::atomic<bool> isAudible { true };

    juce::WeakReference<Selectable> cachedParentTrack;
    FolderTrack* cachedParentFolderTrack = nullptr;

    //==============================================================================
    void updateTrackList();
    void updateCachedParent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};

}} // namespace tracktion { inline namespace engine
