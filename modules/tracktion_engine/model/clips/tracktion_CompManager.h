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
class CompManager   : public juce::SingleThreadedReferenceCountedObject,
                      private juce::ValueTree::Listener
{
public:
    /** Creates a CompManager for a given clip. */
    CompManager (Clip&, const juce::ValueTree&);

    /** Destructor. */
    ~CompManager();

    using Ptr = juce::ReferenceCountedObjectPtr<CompManager>;

    virtual void initialise();

    //==============================================================================
    virtual juce::int64 getBaseTakeHash (int takeIndex) const = 0;
    virtual double getTakeLength (int takeIndex) const = 0;
    virtual double getOffset() const = 0;
    virtual double getLoopLength() const = 0;
    virtual bool getAutoTempo() = 0;
    virtual double getSourceTempo() = 0;
    virtual juce::String getWarning()                   { return {}; }
    virtual float getRenderProgress() const             { return 1.0f; }
    virtual void discardCachedData()                    {}

    /** Triggers the render of the comp. Call this when you change the comp in some way. */
    virtual void triggerCompRender() = 0;

    /** Should flatten the comp and remove all other takes. */
    virtual void flattenTake (int takeIndex, bool deleteSourceFiles) = 0;

    /** Pastes an existing comp to this manager and returns the newly added tree. */
    virtual juce::ValueTree pasteComp (const juce::ValueTree& /*compTree*/) { return {}; }

    //==============================================================================
    Clip& getClip() const noexcept                      { return clip; }

    /** Sets a component to be updated during render processes. */
    virtual void setStripToUpdate (juce::Component*)    {}

    /** Returns true if the source should display a warning about using multi-tempo changes. */
    bool shouldDisplayWarning() const noexcept          { return displayWarning; }

    //==============================================================================
    /** Returns the section at the given index of a given take. */
    juce::ValueTree getSection (int takeIndex, int sectionIndex) const;

    /** Returns either the section for the current comp at a given time or if a whole
        take is being used the take. Check the type to find out which this is.
    */
    juce::ValueTree findSectionAtTime (double time);

    /** Returns the index of the section whose end lies within the given time range.
        This is useful when finding out what edges to drag etc. If no section is found
        then -1 is returned.
    */
    int findSectionWithEndTime (EditTimeRange range, int takeIndex, bool& timeFoundAtStartOfSection) const;

    /** Returns the time range a given section occupies for a given take.
        If there is no segment at the indexes this will return an empty range.
    */
    EditTimeRange getSectionTimes (const juce::ValueTree&) const;

    //==============================================================================
    juce::ValueTree getTakesTree()                      { return takesTree; }

    /** Sets the active take index. This will also update the source. */
    void setActiveTakeIndex (int index);

    /** Returns the active take index. */
    int getActiveTakeIndex() const                      { return clip.getCurrentTake(); }

    /** Returns the active take tree. */
    juce::ValueTree getActiveTakeTree() const           { return takesTree.getChild (getActiveTakeIndex()); }

    /** Returns the number of takes that are not comps. */
    int getNumTakes() const;

    /** Returns the number of comps that are comps. */
    int getNumComps() const                             { return takesTree.getNumChildren() - getNumTakes(); }

    /** Returns the total number of takes including comp takes. */
    int getTotalNumTakes() const                        { return takesTree.getNumChildren(); }

    /** Returns true if the given take at an index is a comp. */
    bool isTakeComp (int takeIndex) const               { return isTakeComp (takesTree.getChild (takeIndex)); }

    /** Returns true if the given take is a comp. */
    bool isTakeComp (const juce::ValueTree& takeTree) const   { return bool (takeTree.getProperty (IDs::isComp, false)); }

    /** Returns true if the current take is a comp. */
    bool isCurrentTakeComp() const                      { return isTakeComp (getActiveTakeIndex()); }

    /** Returns the name of a take. */
    juce::String getTakeName (int index) const;

    //==============================================================================
    /** Adds a new comp to the end of the takes list optionally making it active.
        This is essentially a new take with a new ProjectItemID and a single blank section.
    */
    virtual juce::ValueTree addNewComp() = 0;

    /** Returns the maximum length that a comp can be. This is effectively the length of the longest take. */
    double getMaxCompLength() const                 { return maxCompLength; }

    /** Returns the time range available for comping i.e. taking into account any offset or looped regions. */
    EditTimeRange getCompRange() const;

    /** Returns the effective speed ratio used for displaying waveforms.
        If the source is using auto-tempo with tempo changes this will use an averaged version of this.
    */
    double getSpeedRatio() const;

    /** Returns the current time multiplier in use by the source, either the speed ratio or auto tempo ratio. */
    double getSourceTimeMultiplier() const          { return effectiveTimeMultiplier; }

    /** Returns a hash code representing a take. This can be used to check if a comp has changed
        since it was last generated.
    */
    juce::int64 getTakeHash (int takeIndex) const;

    //==============================================================================
    /** Changes the index of the active comp's section at a given time.
        If the active comp is just a normal take this will change the active take.
    */
    void changeSectionIndexAtTime (double time, int takeIndex);

    /** Removes a section from the comp at the given time if the section is at the given take index. */
    void removeSectionIndexAtTime (double time, int takeIndex);

    /** Moves a section's end time to the new time specified. If this crosses the boundry
        of another section the two sections will be merged.
        @returns    the index of the section that was being dragged
    */
    void moveSectionEndTime (juce::ValueTree& section, double newTime);

    /** Moves a section by the specified time delta also moving the previous section's end time
        by the same ammount. If this covers up another section the covered section will be removed.
        If this drags a section at either end either a new blank section will be created or the drag will be
        limited by the take's bounds.
        If any sections are removed due to an overlap this will return the new section index that this represents.
    */
    void moveSection (juce::ValueTree& section, double timeDelta);

    /** Moves a section to an absolute end time also moving the previous section's end time
        by the same ammount. If this covers up another section the covered section will be removed.
        If this drags a section at either end either a new blank section will be created or the drag will be
        limited by the take's bounds.
        If any sections are removed due to an overlap this will return the new section index that this represents.
    */
    void moveSectionToEndAt (juce::ValueTree& section, double newEndTime);

    /** Adds a new section at a given time and returns the index of it. */
    juce::ValueTree addSection (int takeIndex, double endTime);

    /** Removes a section from the active comp if it is within range. */
    void removeSection (const juce::ValueTree& sectionToRemove);

    /** Find the current section at the given time and splits it in two ready for a new comp section. */
    juce::ValueTree splitSectionAtTime (double time);

    /** Removes all sections which lie within the given time range. Useful when performing drag operations.
        @returns the number of sections removed removed.
    */
    void removeSectionsWithinRange (EditTimeRange timeRange, const juce::ValueTree& sectionToKeep);

protected:
    juce::ValueTree takesTree;
    int lastRenderedTake = -1;
    juce::int64 lastHash = 0;
    double maxCompLength, effectiveTimeMultiplier;
    double lastOffset = 1.0, lastTimeRatio = 1.0;

    juce::UndoManager* getUndoManager() const;
    void keepSectionsSortedAndInRange();
    juce::ValueTree getNewCompTree() const;

private:
    //==============================================================================
    Clip& clip;
    bool displayWarning = false;

    //==============================================================================
    void reCacheInfo();
    void refreshCachedTakeLengths();
    void updateOffsetAndRatioFromSource();
    void addOrRemoveListenerIfNeeded();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree&) override        { if (p == takesTree) reCacheInfo(); }
    void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree&, int) override { if (p == takesTree) reCacheInfo(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override           {}
    void valueTreeParentChanged (juce::ValueTree&) override                         {}
    void valueTreeRedirected (juce::ValueTree&) override                            { jassertfalse; }

    //==============================================================================
    struct RenderTrigger;
    std::unique_ptr<RenderTrigger> renderTrigger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompManager)
};

//==============================================================================
/**
*/
class CompFactory
{
public:
    CompFactory() = default;
    ~CompFactory() { jassert (comps.isEmpty()); }

    CompManager::Ptr getCompManager (Clip&);

private:
    friend class CompManager;
    juce::Array<CompManager*> comps;
    juce::CriticalSection compLock;

    void addComp (CompManager&);
    void removeComp (CompManager&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompFactory)
};

//==============================================================================
/**
*/
class WaveCompManager   : public CompManager,
                          private juce::Timer
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<WaveCompManager>;

    WaveCompManager (WaveAudioClip&);
    ~WaveCompManager();

    /** Updates an array of thumbnails so they represent the takes and are in the correct order etc. */
    void updateThumbnails (juce::Component&, juce::OwnedArray<SmartThumbnail>& thumbnails) const;

    /** Returns the current comp file. */
    juce::File getCurrentCompFile() const;

    //==============================================================================
    juce::int64 getBaseTakeHash (int takeIndex) const override    { return getProjectItemIDForTake (takeIndex).getItemID(); }
    double getTakeLength (int takeIndex) const override;
    double getOffset() const override;
    double getLoopLength() const override;
    bool getAutoTempo() override;
    double getSourceTempo() override;
    juce::String getWarning() override;
    float getRenderProgress() const override;

    void triggerCompRender() override;
    void flattenTake (int takeIndex, bool deleteSourceFiles) override;
    juce::ValueTree pasteComp (const juce::ValueTree& compTree) override;
    void setStripToUpdate (juce::Component* strip) override;

    juce::ValueTree addNewComp() override;

    //==============================================================================
    struct CompRenderContext;

    /** Returns a context to render the current taste of this comp. */
    CompRenderContext* createRenderContext() const;

    /** Renders the comp using the given writer and ThreadPoolJob.
        This will return true if the comp was successfully completed or false if it failed.
        Note that this should only be called for comp takes and will simply return true for full takes.
    */
    static bool renderTake (CompRenderContext&, AudioFileWriter&,
                            juce::ThreadPoolJob&, std::atomic<float>& progress);

private:
    enum { compGeneratorDelay = 500 };

    WaveAudioClip& clip;
    AudioFile lastCompFile;
    juce::String warning;

    //==============================================================================
    struct FlattenRetrier;
    std::unique_ptr<FlattenRetrier> flattenRetrier;

    struct CompUpdater;
    std::unique_ptr<CompUpdater> compUpdater;

    void setProjectItemIDForTake (int takeIndex, ProjectItemID) const;
    ProjectItemID getProjectItemIDForTake (int takeIndex) const;
    AudioFile getSourceFileForTake (int takeIndex) const;
    juce::File getDefaultTakeFile (int index) const;
    ProjectItem::Ptr getOrCreateProjectItemForTake (juce::ValueTree& takeTree);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveCompManager)
};

//==============================================================================
/** */
class MidiCompManager   : public CompManager
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<MidiCompManager>;

    MidiCompManager (MidiClip&);
    ~MidiCompManager();

    void initialise() override;
    MidiList* getSequenceLooped (int index);

    //==============================================================================
    juce::int64 getBaseTakeHash (int takeIndex) const override;
    double getTakeLength (int takeIndex) const override;
    double getOffset() const override                       { return 0.0; }
    double getLoopLength() const override                   { return getTakeLength (0); }
    bool getAutoTempo() override                            { return false; }
    double getSourceTempo() override                        { return 1.0; }
    juce::String getWarning() override                      { return {}; }
    float getRenderProgress() const override                { return 1.0f; }
    void discardCachedData() override;

    void triggerCompRender() override;
    void flattenTake (int takeIndex, bool deleteSourceFiles) override;
    juce::ValueTree addNewComp() override;

private:
    MidiClip& clip;
    juce::ValueTree midiTakes;
    juce::OwnedArray<MidiList> cachedLoopSequences;

    void createComp (const juce::ValueTree& takeTree);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiCompManager)
};

} // namespace tracktion_engine
