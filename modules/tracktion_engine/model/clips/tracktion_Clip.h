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

//==============================================================================
/** Holds a clip's level state.
*/
struct ClipLevel
{
    juce::CachedValue<AtomicWrapper<float>> dbGain, pan;
    juce::CachedValue<AtomicWrapper<bool>> mute;
};

//==============================================================================
/** Provides a thread-safe way to share a clip's levels with an audio engine without
    worrying about the Clip being deleted from underneath you.
*/
struct LiveClipLevel
{
    /** Creates an empty LiveClipLevel. */
    LiveClipLevel() noexcept = default;

    /** Creates a LiveClipLevel from a ClipLevel. */
    LiveClipLevel (std::shared_ptr<ClipLevel> l) noexcept
        : levels (std::move (l)) {}

    /** Returns the clip's absolute gain. */
    float getGain() const noexcept              { return levels ? dbToGain (levels->dbGain.get()) : 1.0f; }

    /** Returns the clip's pan from -1.0 to 1.0. */
    float getPan() const noexcept               { return levels ? static_cast<float> (levels->pan.get()) : 0.0f; }

    /** Returns true if the clip is muted. */
    bool isMute() const noexcept                { return levels && levels->mute.get(); }

    /** Returns the clip's gain if the clip is not muted. */
    float getGainIncludingMute() const noexcept { return isMute() ? 0.0f : getGain(); }

    /** Reutrns the left and right gains taking in to account mute and pan values. */
    void getLeftAndRightGains (float& left, float& right) const noexcept
    {
        const float g = getGainIncludingMute();
        const float pv = getPan() * g;
        left  = g - pv;
        right = g + pv;
    }

private:
    std::shared_ptr<ClipLevel> levels;
};


//==============================================================================
/**
    A clip in an edit.
    This is the base class for various clip types
*/
class Clip   : public TrackItem,
               public Exportable,
               protected juce::ValueTree::Listener
{
public:
    //==============================================================================
    /** Creates a clip of a given type from a ValueTree state.
        Clip's have to have a parent ClipTrack and unique EditItemID @see Edit::createNewItemID
        You would usually create a clip using ClipTrack::insertNewClip
    */
    Clip (const juce::ValueTree&, ClipTrack&, EditItemID, Type);
    
    /** Destructor. */
    ~Clip() override;

    /** Initialises the Clip. Called once automatically after construction. */
    virtual void initialise();

    using Ptr = juce::ReferenceCountedObjectPtr<Clip>;
    using Array = juce::ReferenceCountedArray<Clip>;

    //==============================================================================
    /** Checks whether a ValueTree is some kind of clip state */
    static bool isClipState (const juce::ValueTree&);
    /** Checks whether a ValueTree type is some kind of clip state */
    static bool isClipState (const juce::Identifier&);

    /** Creates a clip for a given ValueTree representation.
        This may return a previously-existing clip with the same ID.
    */
    static Ptr createClipForState (const juce::ValueTree&, ClipTrack& targetTrack);

    /** Can be overridden to ensure any state (e.g. clip plugins) is flushed to the ValueTree ready for saving. */
    virtual void flushStateToValueTree();

    /** Called when the source media file reference (attribute "source")
        has changed - i.e. when the clip has a new ProjectItemID assigned, not when the
        file itself changes.
    */
    virtual void sourceMediaChanged();

    /** Called when there are pitch or tempo changes made which might require clips
        to adjust timing information.
    */
    virtual void pitchTempoTrackChanged() {}

    //==============================================================================
    /** Returns the name of the clip. */
    virtual juce::String getName() override             { return clipName; }
    /** Sets a new name for a clip. */
    void setName (const juce::String& newName);

    /** Returns true if this is a MidiClip. */
    virtual bool isMidi() const = 0;
    /** Tests whether this clip can go on the given track. */
    virtual bool canGoOnTrack (Track&) = 0;

    //==============================================================================
    /** True if it references a source file - i.e. audio clips do, midi doesn't. */
    virtual bool usesSourceFile()                       { return false; }

    /** Returns the SourceFileReference of the Clip. */
    SourceFileReference& getSourceFileReference()       { return sourceFileReference; }

    /** Returns the current source file, this is different to the SourceFileReference
        as it could be a temporary comp file, clipFX, reverse render etc.
    */
    juce::File getCurrentSourceFile() const             { return currentSourceFile; }

    //==============================================================================
    /** Returns an array of any ReferencedItem[s] e.g. audio files. */
    juce::Array<ReferencedItem> getReferencedItems() override  { return {}; }

    /** Should be implemented to change the underlying source to a new ProjectItemID. */
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID /*newID*/, double /*newStartTime*/) override {}

    //==============================================================================
    /** Returns the ClipPosition on the parent Track. */
    ClipPosition getPosition() const override;

    /** Returns the beat number (with offset) at the given time */
    BeatPosition getContentBeatAtTime (TimePosition time) const;
    /** Returns time of a beat number */
    TimePosition getTimeOfContentBeat (BeatPosition beat) const;

    /** Returns the maximum lenght this clip can have. */
    virtual TimeDuration getMaximumLength()               { return toDuration (Edit::getMaximumEditEnd()); }

    /** Returns times for snapping to, relative to the Edit. Base class adds start and end time. */
    virtual juce::Array<TimePosition> getInterestingTimes();

    /** Returns the first marked time in the source file which can be used for
        syncronising newly added clips.
    */
    TimePosition getSpottingPoint() const;

    //==============================================================================
    /** Returns true if this clip is capable of looping. */
    virtual bool canLoop() const                    { return false; }
    /** Returns true if this clip is currently looping. */
    virtual bool isLooping() const                  { return false; }
    /** Returns true if this clip's looping is based on beats or false if absolute time. */
    virtual bool beatBasedLooping() const           { return false; }
    /** Sets the clip looping a number of times. */
    virtual void setNumberOfLoops (int)             {}
    /** Disables all looping. */
    virtual void disableLooping()                   {}

    /** Returns the beat position of the loop start point. */
    virtual BeatPosition getLoopStartBeats() const          { return BeatPosition(); }
    /** Returns the start time of the loop start point. */
    virtual TimePosition getLoopStart() const               { return TimePosition(); }
    /** Returns the length of loop in beats. */
    virtual BeatDuration getLoopLengthBeats() const         { return BeatDuration(); }
    /** Returns the length of loop in seconds. */
    virtual TimeDuration getLoopLength() const              { return TimeDuration(); }

    /** Returns the loop range in seconds. */
    TimeRange getLoopRange() const                          { return { getLoopStart(), getLoopLength() }; }
    /** Returns the loop range in beats. */
    BeatRange getLoopRangeBeats() const                     { return { getLoopStartBeats(), getLoopLengthBeats() }; }

    /** Sets the loop range the clip should use in seconds. */
    virtual void setLoopRange (TimeRange)                   {}
    /** Sets the loop range the clip should use in beats. */
    virtual void setLoopRangeBeats (BeatRange)              {}

    /** Returns true if the clip is muted. */
    virtual bool isMuted() const = 0;
    /** Mutes or unmutes the clip. */
    virtual void setMuted (bool) {}

    /** Determines the clip sync type. */
    enum SyncType
    {
        syncBarsBeats = 0,  /**< Sync to beats. */
        syncAbsolute        /**< Sync to abslute time.*/
    };

    /** Sets the sync type for the clip. */
    virtual void setSyncType (SyncType sync)        { syncType = sync; }
    /** Returns the sync type clip is using. */
    SyncType getSyncType() const                    { return syncType; }

    //==============================================================================
    /** Sets the position of the clip. */
    void setPosition (ClipPosition newPosition);

    /** Sets the start time of the clip.
        @param newStart     The start time in seconds
        @param preserveSync Whether the source material position should be kept static in relation to the Edit's timeline.
        @param keepLength   Whether the end should be moved to keep the same length.
    */
    void setStart (TimePosition newStart, bool preserveSync, bool keepLength);

    /** Sets the length of the clip.
        @param newLength    The length in seconds
        @param preserveSync Whether the source material position should be kept static in relation to the Edit's timeline.
    */
    void setLength (TimeDuration newLength, bool preserveSync);

    /** Sets the end of the clip.
        @param newEnd       The end time in seconds
        @param preserveSync Whether the source material position should be kept static in relation to the Edit's timeline.
    */
    void setEnd (TimePosition newEnd, bool preserveSync);

    /** Sets the offset of the clip, i.e. how much the clip's content should be shifted within the clip boundary.
        @param newOffset    The offset in seconds
    */
    void setOffset (TimeDuration newOffset);

    /** Trims away any part of the clip that overlaps this region. */
    void trimAwayOverlap (TimeRange editRangeToTrim);

    /** Removes this clip from the parent track. */
    void removeFromParentTrack();

    /** Moves the clip to a new Track (if possible).
        @returns true if the clip could be moved.
    */
    bool moveToTrack (Track&);

    //==============================================================================
    /** Returns the speed ratio i.e. how quickly the clip plays back. */
    double getSpeedRatio() const noexcept           { return speedRatio; }
    
    /** Sets a speed ratio i.e. how quickly the clip plays back. */
    virtual void setSpeedRatio (double);

    /** stretches and scales this clip relative to a fixed point in the edit.
        @param pivotTimeInEdit  The time to keep fixed
        @param factor           The scale factor
    */
    virtual void rescale (TimePosition pivotTimeInEdit, double factor);

    //==============================================================================
    /** Returns true if the clip is part of a group. */
    bool isGrouped() const override                 { return groupID.get().isValid(); }
    /** Returns the parent TrackItem if part of a group. */
    TrackItem* getGroupParent() const override;
    /** Sets the clip to be part of a group. */
    void setGroup (EditItemID newGroupID);
    /** Returns the ID of the group. */
    EditItemID getGroupID() const noexcept          { return groupID; }
    /** Returns this as a CollectionClip if it is one. */
    CollectionClip* getGroupClip() const;

    //==============================================================================
    /** Returns true if this clip is linked with any others. */
    bool isLinked() const                           { return linkID.get().isNotEmpty(); }
    /** Sets the link ID to link this clip with others. */
    void setLinkGroupID (juce::String newLinkID)    { linkID = newLinkID; }
    /** Returns the link ID of this clip. */
    juce::String getLinkGroupID() const             { return linkID; }

    //==============================================================================
    /** Returns the parent ClipTrack this clip is on. */
    ClipTrack* getClipTrack() const                 { return track; }
    /** Returns the parent Track this clip is on. */
    Track* getTrack() const override;

    //==============================================================================
    /** Returns the colour property of this clip. */
    virtual juce::Colour getColour() const;
    /** Sets the colour property of this clip. */
    void setColour (juce::Colour col)               { colour = col; }

    //==============================================================================
    /** Removes the given plugin from the clip if the clip supports plugins. */
    virtual void removePlugin (const Plugin::Ptr&)             {}

    /** Adds a plugin to the clip.
        @returns false if the clip contain plugins.
    */
    virtual bool addClipPlugin (const Plugin::Ptr&, SelectionManager&)   { return false; }

    /** Returns all the plugins on the clip. */
    virtual Plugin::Array getAllPlugins()                      { return {}; }
    /** Sends an update to all plugins mirroing the one passed in. */
    virtual void sendMirrorUpdateToAllPlugins (Plugin&) const  {}

    /** Returns the PluginList for this clip if it has one. */
    virtual PluginList* getPluginList()                        { return {}; }

    //==============================================================================
    /** Returns the default colour for this clip. */
    virtual juce::Colour getDefaultColour() const = 0;

    //==============================================================================
    /** Clears any takes this clip has. */
    virtual void clearTakes()                               {}
    /** Returns true if this clip has any takes. */
    virtual bool hasAnyTakes() const                        { return false; }
    /** Returns the descriptions of any takes. */
    virtual juce::StringArray getTakeDescriptions() const   { return {}; }
    /** Sets a given take index to be the current take. */
    virtual void setCurrentTake (int /*takeIndex*/)         {}
    /** Returns the current take index. */
    virtual int getCurrentTake() const                      { return 0; }
    /** Returns the total number of takes.
        @param includeComps Whether comps should be included in the count
    */
    virtual int getNumTakes (bool /*includeComps*/)         { return 0; }
    /** Returns true if the current take is a comp. */
    virtual bool isCurrentTakeComp()                        { return false; }

    /** Sets whether the clip should be showing takes. */
    virtual void setShowingTakes (bool shouldShow)          { showingTakes = shouldShow; }
    /** Returns true if the clip is showing takes. */
    virtual bool isShowingTakes() const                     { return showingTakes;  }
    
    /** Attempts to unpack the takes to new clips.
        @param toNewTracks  If true this will create new tracks for the new clips,
                            otherwise they'll be placed on existing tracks
    */
    virtual Clip::Array unpackTakes (bool /*toNewTracks*/)  { return {}; }

    //==============================================================================
    /** Clones the given clip to this clip. */
    virtual void cloneFrom (Clip*);

    /** Triggers a call to cloneFrom for all clips with the same linkID.
        @see setLinkGroupID
    */
    void updateLinkedClips();

    //==============================================================================
    /** Returns the PatternGenerator for this clip if it has one.
        @see MidiClip
    */
    virtual PatternGenerator* getPatternGenerator() { return {}; }

    //==============================================================================
    /** Listener interface to be notified of recorded MIDI being sent to the plugins. */
    struct Listener
    {
        /* Destructor */
        virtual ~Listener() = default;

        /** Called when a recorded MidiMessage has been generated and sent.
            to the playback graph. This applies to Step and Midi clips.
         */
        virtual void midiMessageGenerated (Clip&, const juce::MidiMessage&) = 0;
    };

    /** Adds a Listener. */
    void addListener (Listener*);

    /** Removes a Listener. */
    void removeListener (Listener*);

    /** Returns the listener list so Nodes can manually call them. */
    juce::ListenerList<Listener>& getListeners()            { return listeners; }

    //==============================================================================
    /** @internal */
    void changed() override;

    /** Returns the UndoManager. @see Edit::getUndoManager. */
    juce::UndoManager* getUndoManager() const;

    juce::ValueTree state;                      /**< The ValueTree of the Clip state. */
    juce::CachedValue<juce::Colour> colour;     /**< The colour property. */

protected:
    friend class Track;
    friend class ClipTrack;
    friend class CollectionClip;

    bool isInitialised = false;
    bool cloneInProgress = false;
    juce::CachedValue<juce::String> clipName;
    ClipTrack* track = nullptr;
    juce::CachedValue<TimePosition> clipStart;
    juce::CachedValue<TimeDuration> length, offset;
    juce::CachedValue<double> speedRatio;
    SourceFileReference sourceFileReference;
    juce::CachedValue<EditItemID> groupID;
    juce::CachedValue<juce::String> linkID;
    juce::File currentSourceFile;
    juce::CachedValue<SyncType> syncType;
    juce::CachedValue<bool> showingTakes;
    std::unique_ptr<PatternGenerator> patternGenerator;
    AsyncCaller updateLinkedClipsCaller;

    juce::ListenerList<Listener> listeners;

    /** Sets a new source file for this clip. */
    void setCurrentSourceFile (const juce::File&);

    /** Moves this clip to a new ClipTrack. */
    virtual void setTrack (ClipTrack*);

    /** Returns the mark points relative to the start of the clip, rescaled to the current speed. */
    virtual juce::Array<TimePosition> getRescaledMarkPoints() const;

    /** @internal */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    /** @internal */
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    /** @internal */
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    /** @internal */
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    /** @internal */
    void valueTreeParentChanged (juce::ValueTree&) override;

private:
    void updateParentTrack();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Clip)
};


//==============================================================================
/** Constraints for clip speeds. */
namespace ClipConstants
{
    const double speedRatioMin = 0.01;  /**< Minimum speed ratio. */
    const double speedRatioMax = 20.0;  /**< Maximum speed ratio. */
}

}} // namespace tracktion { inline namespace engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion::engine::Clip::SyncType>
    {
        static tracktion::engine::Clip::SyncType fromVar (const var& v)   { return (tracktion::engine::Clip::SyncType) static_cast<int> (v); }
        static var toVar (tracktion::engine::Clip::SyncType v)            { return static_cast<int> (v); }
    };
}
