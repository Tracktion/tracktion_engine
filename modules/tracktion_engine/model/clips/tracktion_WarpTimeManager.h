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
//==============================================================================
/**
    A WarpMarker is a point that maps from a linear "source" time to a "warped"
    time.
*/
struct WarpMarker
{
    /** Creates an empty WarpMarker. */
    WarpMarker() noexcept = default;

    /** Creates a WarpMarker with a source and warp time. */
    WarpMarker (TimePosition s, TimePosition w)
        : sourceTime (s), warpTime (w) {}

    /** Loads a WarpMarker from a saved state. */
    WarpMarker (const juce::ValueTree& v)  : state (v)
    {
        updateFrom (v, IDs::sourceTime);
        updateFrom (v, IDs::warpTime);
    }

    /** Updates this WarpMarker from a ValueTree property. */
    void updateFrom (const juce::ValueTree& v, const juce::Identifier& i)
    {
        if (i == IDs::sourceTime)
            sourceTime = TimePosition::fromSeconds (double (v.getProperty (IDs::sourceTime)));
        else if (i == IDs::warpTime)
            warpTime = TimePosition::fromSeconds (double (v.getProperty (IDs::warpTime)));
    }

    /** Returns a hash for this marker. */
    HashCode getHash() const noexcept;

    juce::ValueTree state;
    TimePosition sourceTime, warpTime;
};


//==============================================================================
//==============================================================================
/**
    A WarpTimeManager contains a list of WarpMarkers and some source material and
    maps times from a linear "view" time to a "wapred" source time.
    
    Once created this can be used to generate an AudioSegmentList to play the
    source material back warped.
*/
class WarpTimeManager : public juce::SingleThreadedReferenceCountedObject,
                        public RenderManager::Job::Listener
{
public:
    //==============================================================================
    using Ptr = juce::ReferenceCountedObjectPtr<WarpTimeManager>;

    /** Creates a WarpTimeManager to warp a clip. */
    WarpTimeManager (AudioClipBase&);

    /** Creates a WarpTimeManager to warp an audio file.
        The WarpMarker state will get added to the parentTree as a child.
    */
    WarpTimeManager (Edit&, const AudioFile& sourceFile, juce::ValueTree parentTree);
    
    /** Destructor. */
    ~WarpTimeManager() override;

    //==============================================================================
    /** Sets a source fiel to warp. */
    void setSourceFile (const AudioFile&);
    
    /** Returns the current source file. */
    AudioFile getSourceFile() const;
    
    /** Returns the length of the source file. */
    TimeDuration getSourceLength() const;

    //==============================================================================
    /** Returns true if the end marker is being used as the end of the source material.
        @see setWarpEndMarkerTime
    */
    bool isWarpEndMarkerEnabled() const noexcept                { return endMarkerEnabled; }

    /** Returns true if the markers are limited to the end of the source length.
        @see getSourceLength
    */
    bool areEndMarkersLimited() const noexcept                  { return endMarkersLimited; }

    /** Returns a list of the current WarpMarkers. */
    const juce::Array<WarpMarker*>& getMarkers() const          { return markers->objects; }

    /** Inserts a new WarpMarker. */
    int insertMarker (WarpMarker);
    
    /** Removes a WarpMarker at a given index. */
    void removeMarker (int index);
    
    /** Removes all WarpMarkers. */
    void removeAllMarkers();
    
    /** Moves a WarpMarker at a given index to a new time. */
    TimePosition moveMarker (int index, TimePosition newWarpTime);

    /** Sets the end time of the source material.
        Only functional if isWarpEndMarkerEnabled returns true.
    */
    void setWarpEndMarkerTime (TimePosition endTime);

    //==============================================================================
    /** Time region can be longer than the clip and the returned array will loop over the clip to match the length */
    juce::Array<TimeRange> getWarpTimeRegions (TimeRange overallTimeRegion) const;
    
    /** Returns an array of transient times that have been detected from the source file.
        The bool here will be false if the detection job hasn't finished running yet so call it again peridically
        until it is true.
    */
    std::pair<bool, juce::Array<TimePosition>> getTransientTimes() const    { return transientTimes; }

    /** Converts a warp time (i.e. a linear time) to the time in the source file after warping has been applied. */
    TimePosition warpTimeToSourceTime (TimePosition warpTime) const;

    /** Converts a source time (i.e. time in the file) to a linear time after warping has been applied. */
    TimePosition sourceTimeToWarpTime (TimePosition sourceTime) const;

    /** Returns the start time of the warped region (can be -ve) */
    TimePosition getWarpedStart() const;

    /** Returns the endTime of the entire warped region */
    TimePosition getWarpedEnd() const;

    /** Sets position in warped region of the redered file end point */
    TimePosition getWarpEndMarkerTime() const;

    /** Returns a hash representing this warp list. */
    HashCode getHash() const;

    /** @internal */
    void editFinishedLoading();

    Edit& edit;
    juce::ValueTree state;

private:
    struct WarpMarkerList   : public ValueTreeObjectList<WarpMarker>
    {
        WarpMarkerList (const juce::ValueTree& v)
            : ValueTreeObjectList<WarpMarker> (v), state (v)
        {
            rebuildObjects();
        }

        ~WarpMarkerList() override
        {
            freeObjects();
        }

        bool isSuitableType (const juce::ValueTree& v) const override   { return v.hasType (IDs::WARPMARKER); }
        WarpMarker* createNewObject (const juce::ValueTree& v) override { return new WarpMarker (v); }
        void deleteObject (WarpMarker* wm) override                     { delete wm; }
        void newObjectAdded (WarpMarker*) override                      {}
        void objectRemoved (WarpMarker*) override                       {}
        void objectOrderChanged() override                              {}

        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
        {
            if (v.hasType (IDs::WARPMARKER))
                if (auto wm = getWarpMarkerFor (v))
                    wm->updateFrom (v, i);
        }

        juce::ValueTree state;

    private:
        WarpMarker* getWarpMarkerFor (const juce::ValueTree& v)
        {
            jassert (v.hasType (IDs::WARPMARKER));
            return objects[v.getParent().indexOf (v)];
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpMarkerList)
    };

    friend class WarpTimeFactory;

    const AudioClipBase* clip = nullptr;
    AudioFile sourceFile;
    bool endMarkerEnabled = true, endMarkersLimited = false;

    std::unique_ptr<WarpMarkerList> markers;
    RenderManager::Job::Ptr transientDetectionJob;
    std::pair<bool, juce::Array<TimePosition>> transientTimes { false, {} };
    std::unique_ptr<Edit::LoadFinishedCallback<WarpTimeManager>> editLoadedCallback;

    juce::UndoManager* getUndoManager() const;
    void jobFinished (RenderManager::Job& job, bool completedOk) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeManager)
};


//==============================================================================
//==============================================================================
/**
    A WarpTimeFactory manages WarpTimeManagers for Clips living in an Edit.
*/
class WarpTimeFactory
{
public:
    /** Constructs the WarpTimeFactory. */
    WarpTimeFactory() = default;
    
    /** Destructor. */
    ~WarpTimeFactory()  { jassert (warpTimeManagers.isEmpty()); }

    /** Returns a WarpTimeManager for a given clip.
        If there are any existing references to a WarpTimeManager for this clip
        this will return the same instance. Otherwise an instance will be created
        for this Clip.
    */
    WarpTimeManager::Ptr getWarpTimeManager (const Clip&);

private:
    friend class WarpTimeManager;
    juce::Array<WarpTimeManager*> warpTimeManagers;
    juce::CriticalSection warpTimeLock;

    void addWarpTimeManager (WarpTimeManager&);
    void removeWarpTimeManager (WarpTimeManager&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeFactory)
};

}} // namespace tracktion { inline namespace engine
