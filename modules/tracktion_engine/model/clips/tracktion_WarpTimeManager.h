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
struct WarpMarker
{
    WarpMarker() noexcept {}
    WarpMarker (double s, double w) : sourceTime (s), warpTime (w) {}

    WarpMarker (const juce::ValueTree& v) : state (v)
    {
        updateFrom (v, IDs::sourceTime);
        updateFrom (v, IDs::warpTime);
    }

    void updateFrom (const juce::ValueTree& v, const juce::Identifier& i)
    {
        if (i == IDs::sourceTime)
            sourceTime = double (v.getProperty (IDs::sourceTime));
        else if (i == IDs::warpTime)
            warpTime = double (v.getProperty (IDs::warpTime));
    }

    juce::int64 getHash() const noexcept;

    juce::ValueTree state;
    double sourceTime = 0, warpTime = 0;
};

//==============================================================================
/**
*/
class WarpTimeManager : public juce::SingleThreadedReferenceCountedObject,
                        public RenderManager::Job::Listener
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<WarpTimeManager>;

    WarpTimeManager (AudioClipBase& source);
    WarpTimeManager (Edit&, const AudioFile& sourceFile, juce::ValueTree parentTree);
    ~WarpTimeManager();

    void setSourceFile (const AudioFile&);
    AudioFile getSourceFile() const;
    double getSourceLength() const;

    void setWarpEndMarkerEnabled (bool);
    bool isWarpEndMarkerEnabled() const noexcept                { return endMarkerEnabled; }

    void setEndMarkersLimited (bool);
    bool areEndMarkesLimited() const noexcept                   { return endMarkersLimited; }

    const juce::Array<WarpMarker*>& getMarkers() const          { return markers->objects; }

    /** if index is -1 it will go on the end of the list */
    void insertMarker (int index, WarpMarker marker);
    void removeMarker (int index);
    void removeAllMarkers();
    double moveMarker (int index, double newWarpTime);
    void setWarpEndMarkerTime (double endTime);

    /** Time region can be longer than the clip and the returned array will loop over the clip to match the length */
    juce::Array<EditTimeRange> getWarpTimeRegions (EditTimeRange overallTimeRegion) const;
    std::pair<bool, juce::Array<double>> getTransientTimes() const    { return transientTimes; }

    double warpTimeToSourceTime (double warpTime) const;
    double sourceTimeToWarpTime (double sourceTime) const;

    /** returns the start time of the warped region (can be -ve) */
    double getWarpedStart() const;

    /** returns the endTime of the entire warped region */
    double getWarpedEnd() const;

    /** sets position in warped region of the redered file end point */
    double getWarpEndMarkerTime() const;

    juce::int64 getHash() const;

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

        ~WarpMarkerList()
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
                if (WarpMarker* wm = getWarpMarkerFor (v))
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

    juce::ScopedPointer<WarpMarkerList> markers;
    RenderManager::Job::Ptr transientDetectionJob;
    std::pair<bool, juce::Array<double>> transientTimes { false, {} };
    juce::ScopedPointer<Edit::LoadFinishedCallback<WarpTimeManager>> editLoadedCallback;

    juce::UndoManager* getUndoManager() const;
    void jobFinished (RenderManager::Job& job, bool completedOk) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeManager)
};

//==============================================================================
class WarpTimeFactory
{
public:
    WarpTimeFactory() = default;
    ~WarpTimeFactory()  { jassert (warpTimeManagers.isEmpty()); }

    WarpTimeManager::Ptr getWarpTimeManager (const Clip&);

private:
    friend class WarpTimeManager;
    juce::Array<WarpTimeManager*> warpTimeManagers;
    juce::CriticalSection warpTimeLock;

    void addWarpTimeManager (WarpTimeManager&);
    void removeWarpTimeManager (WarpTimeManager&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeFactory)
};

} // namespace tracktion_engine
