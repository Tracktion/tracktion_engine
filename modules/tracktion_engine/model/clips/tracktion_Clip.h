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
    A clip in an edit.
    This is the base class for various clip types
*/
class Clip   : public TrackItem,
               public Exportable,
               protected juce::ValueTree::Listener
{
public:
    //==============================================================================
    Clip (const juce::ValueTree&, ClipTrack&, EditItemID, Type);
    ~Clip();

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

    virtual void flushStateToValueTree();

    /** this is called when the source media file reference (attribute "source")
        has changed - i.e. when the clip has a new media id assigned, not when the
        file itself changes.
    */
    virtual void sourceMediaChanged();

    virtual void pitchTempoTrackChanged() {}

    //==============================================================================
    virtual juce::String getName() override             { return clipName; }
    void setName (const juce::String& newName);

    virtual bool isMidi() const = 0;
    virtual bool canGoOnTrack (Track&) = 0;

    //==============================================================================
    /** true if it references a source file - i.e. audio clips do, midi doesn't. */
    virtual bool usesSourceFile()                       { return false; }

    /** Returns the SourceFileReference of the Clip. */
    SourceFileReference& getSourceFileReference()       { return sourceFileReference; }

    /** Returns the current source file, this is different to the SourceFileReference
        as it could be a temporary comp file, clipFX, reverse render etc.
    */
    juce::File getCurrentSourceFile() const             { return currentSourceFile; }

    //==============================================================================
    juce::Array<ReferencedItem> getReferencedItems() override  { return {}; }
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID /*newID*/, double /*newStartTime*/) override {}

    //==============================================================================
    ClipPosition getPosition() const override;

    /** Returns the beat number (with offset) at the given time */
    double getContentBeatAtTime (double time) const;
    /** Returns time of a beat number */
    double getTimeOfContentBeat (double beat) const;

    virtual double getMaximumLength()               { return Edit::maximumLength; }

    /** Returns times for snapping to, relative to the edit. Base class adds start and end time. */
    virtual juce::Array<double> getInterestingTimes();

    double getSpottingPoint() const;

    //==============================================================================
    virtual bool canLoop() const                    { return false; }
    virtual bool isLooping() const                  { return false; }
    virtual bool beatBasedLooping() const           { return false; }
    virtual void setNumberOfLoops (int)             {}
    virtual void disableLooping()                   {}

    virtual double getLoopStartBeats() const        { return 0.0; }
    virtual double getLoopStart() const             { return 0.0; }
    virtual double getLoopLengthBeats() const       { return 0.0; }
    virtual double getLoopLength() const            { return 0.0; }

    virtual void setLoopRange (EditTimeRange newLoopRange)            { juce::ignoreUnused (newLoopRange); }
    virtual void setLoopRangeBeats (juce::Range<double> newBeatRange) { juce::ignoreUnused (newBeatRange); }

    virtual bool isMuted() const = 0;
    virtual void setMuted (bool) {}

    enum SyncType
    {
        syncBarsBeats = 0,
        syncAbsolute
    };

    virtual void setSyncType (SyncType sync)        { syncType = sync; }
    SyncType getSyncType() const                    { return syncType; }

    //==============================================================================
    void setPosition (ClipPosition newPosition);
    void setStart (double newStart, bool preserveSync, bool keepLength);
    void setLength (double newLength, bool preserveSync);
    void setEnd (double newEnd, bool preserveSync);
    void setOffset (double newOffset);

    /** trims away any part of the clip that overlaps this region. */
    void trimAwayOverlap (EditTimeRange editRangeToTrim);

    void removeFromParentTrack();
    bool moveToTrack (Track&);

    double getSpeedRatio() const noexcept           { return speedRatio; }
    virtual void setSpeedRatio (double);

    // stretches and scales this clip relative to a fixed point in the edit.
    virtual void rescale (double pivotTimeInEdit, double factor);

    //==============================================================================
    bool isGrouped() const override                 { return groupID.get().isValid(); }
    TrackItem* getGroupParent() const override;
    void setGroup (EditItemID newGroupID)           { groupID = newGroupID; }
    EditItemID getGroupID() const noexcept          { return groupID; }
    CollectionClip* getGroupClip() const;

    //==============================================================================
    bool isLinked() const                           { return linkID.get().isValid(); }
    void setLinkGroupID (EditItemID newLinkID)      { linkID = newLinkID; }
    EditItemID getLinkGroupID() const               { return linkID; }

    //==============================================================================
    ClipTrack* getClipTrack() const                 { return track; }
    Track* getTrack() const override;

    //==============================================================================
    virtual juce::Colour getColour() const;
    void setColour (juce::Colour col)               { colour = col; }

    //==============================================================================
    /** if the clip can contain plugins, it should implement this */
    virtual void removePlugin (const Plugin::Ptr&)             {}

    /** returns false if the clip can't do this */
    virtual bool addClipPlugin (const Plugin::Ptr&, SelectionManager&)   { return false; }

    virtual Plugin::Array getAllPlugins()                      { return {}; }
    virtual void sendMirrorUpdateToAllPlugins (Plugin&) const  {}

    virtual PluginList* getPluginList()                        { return {}; }

    //==============================================================================
    // Creates the audio node that will be used to play/render this clip.
    virtual AudioNode* createAudioNode (const CreateAudioNodeParams&) = 0;

    //==============================================================================
    virtual juce::Colour getDefaultColour() const = 0;

    //==============================================================================
    virtual void clearTakes()                               {}
    virtual bool hasAnyTakes() const                        { return false; }
    virtual juce::StringArray getTakeDescriptions() const   { return {}; }
    virtual void setCurrentTake (int /*takeIndex*/)         {}
    virtual int getCurrentTake() const                      { return 0; }
    virtual int getNumTakes (bool /*includeComps*/)         { return 0; }
    virtual bool isCurrentTakeComp()                        { return false; }

    virtual void setShowingTakes (bool shouldShow)          { showingTakes = shouldShow; }
    virtual bool isShowingTakes() const                     { return showingTakes;  }
    virtual Clip::Array unpackTakes (bool)                  { return {}; }

    //==============================================================================
    virtual void cloneFrom (Clip*);

    void updateLinkedClips();
    void showLoopMenu();

    //==============================================================================
    virtual PatternGenerator* getPatternGenerator() { return {}; }

    //==============================================================================
    void changed() override;

    juce::UndoManager* getUndoManager() const;

    juce::ValueTree state;
    juce::CachedValue<juce::Colour> colour;

protected:
    friend class Track;
    friend class ClipTrack;
    friend class CollectionClip;

    bool isInitialised = false;
    bool cloneInProgress = false;
    juce::CachedValue<juce::String> clipName;
    ClipTrack* track = nullptr;
    juce::CachedValue<double> clipStart, length, offset, speedRatio;
    SourceFileReference sourceFileReference;
    juce::CachedValue<EditItemID> groupID, linkID;
    juce::File currentSourceFile;
    juce::CachedValue<SyncType> syncType;
    juce::CachedValue<bool> showingTakes;
    juce::ScopedPointer<PatternGenerator> patternGenerator;
    AsyncCaller updateLinkedClipsCaller;

    void setCurrentSourceFile (const juce::File&);
    virtual void setTrack (ClipTrack*);
    void updateParentTrack();

    // returns the mark points relative to the start of the clip, rescaled to the current speed
    virtual juce::Array<double> getRescaledMarkPoints() const;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Clip)
};


//==============================================================================
namespace ClipConstants
{
    const double speedRatioMin = 0.01;
    const double speedRatioMax = 20.0;
}

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::Clip::SyncType>
    {
        static tracktion_engine::Clip::SyncType fromVar (const var& v)   { return (tracktion_engine::Clip::SyncType) static_cast<int> (v); }
        static var toVar (tracktion_engine::Clip::SyncType v)            { return static_cast<int> (v); }
    };
}
