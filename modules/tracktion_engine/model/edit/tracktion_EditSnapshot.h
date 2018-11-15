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
    Holds a snapshot of an Edit file of the last time it was saved.
*/
class EditSnapshot  : public juce::ReferenceCountedObject
{
public:
    //==============================================================================
    using Ptr = juce::ReferenceCountedObjectPtr<EditSnapshot>;

    static Ptr getEditSnapshot (ProjectItemID);

    //==============================================================================
    struct Marker
    {
        juce::String name;
        juce::Colour colour;
        EditTimeRange time;
    };

    //==============================================================================
    ~EditSnapshot();

    /** Returns the ProjectItemID. */
    ProjectItemID getID() const noexcept                { return itemID; }

    /** Returns the File if this was created from one. */
    juce::File getFile() const                          { return sourceFile; }

    /** Returns the source Xml. */
    juce::ValueTree getState() noexcept                 { return state; }

    /** Sets the Edit XML that the XmlEdit should refer to.
        This will take ownership of the XmlElement so don't hang on to it.
        Once this is set you can retrieve the Xml for saving etc. using getXml().
        As the length isn't stored in the Xml we need to provide it.

        Note that this DOES NOT update the cached properties, once the Xml has been set call
        refreshCacheAndSave to update these. Although this seems a bit convoluted it's how the
        temp Edit files work so we need to follow this. It means that this will hold up-to-date
        Xml but only cached properties of the last save which can be used in things like EditClips.
    */
    void setState (juce::ValueTree, double editLength);

    /** Returns true if the current source is a valid Edit. */
    bool isValid() const;

    /** Looks in the ProjectManager for the relevant ProjectItem and updates it's state to reflect this. */
    void refreshFromProjectManager();

    /** Refreshes the cached properties and calls any listeners. */
    void refreshCacheAndNotifyListeners();

    /** deals only with snapshots so it's relatively fast. */
    juce::ReferenceCountedArray<EditSnapshot> getNestedEditObjects();

    //==============================================================================
    // Set of cached properties of the last saved state
    juce::String getName() const                                        { return name; }
    int getNumTracks() const                                            { return numTracks; }
    int getNumAudioTracks() const                                       { return numAudioTracks; }
    int getIndexOfTrackID (EditItemID trackID) const                    { return trackIDs.indexOf (trackID); }
    juce::String getTrackName (int index) const                         { return trackNames[index]; }
    juce::String getTrackNameFromID (EditItemID trackID) const          { return trackNames[getIndexOfTrackID(trackID)]; }
    bool isAudioTrack (int trackIndex) const                            { return audioTracks[trackIndex]; }
    bool isTrackMuted (int trackIndex) const                            { return mutedTracks[trackIndex]; }
    bool isTrackSoloed (int trackIndex) const                           { return soloedTracks[trackIndex]; }
    bool isTrackSoloIsolated (int trackIndex) const                     { return soloIsolatedTracks[trackIndex]; }
    int audioToGlobalTrackIndex (int audioIndex) const;

    double getLength() const                                            { return length; }
    double getMarkIn() const                                            { return markIn; }
    double getMarkOut() const                                           { return markOut; }
    bool areMarksActive() const                                         { return marksActive; }

    double getTempo() const                                             { return tempo; }
    int getTimeSigNumerator() const                                     { return timeSigNumerator; }
    int getTimeSigDenominator() const                                   { return timeSigDenominator; }
    int getPitch() const                                                { return pitch; }

    const juce::Array<Marker>& getMarkers() const                       { return markers; }
    const juce::Array<EditItemID>& getTracks() const                    { return trackIDs; }
    const juce::Array<ProjectItemID>& getEditClips() const              { return editClipIDs; }
    const juce::Array<ProjectItemID>& getClipsSourceIDs() const         { return clipSourceIDs; }
    juce::int64 getHash() const                                         { return lastSaveTime.toMilliseconds(); }

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void editChanged (EditSnapshot&) = 0;
    };

    void addListener (Listener* l)          { listeners.add (l); }
    void removeListener (Listener* l)       { listeners.remove (l); }

private:
    //==============================================================================
    struct ListHolder;
    std::unique_ptr<ListHolder> listHolder;

    ProjectItemID itemID;
    juce::File sourceFile;
    juce::ValueTree state;
    juce::Time lastSaveTime;

    juce::String name;
    int numTracks, numAudioTracks;
    juce::StringArray trackNames;
    juce::BigInteger audioTracks, mutedTracks, soloedTracks, soloIsolatedTracks;

    juce::Array<EditItemID> trackIDs;
    juce::Array<ProjectItemID> editClipIDs, clipSourceIDs;
    double length = 0.0, markIn = 0.0, markOut = 0.0, tempo = 0.0;
    bool marksActive = false;
    int timeSigNumerator = 4, timeSigDenominator = 4, pitch = 60;
    juce::Array<Marker> markers;

    juce::ListenerList<Listener> listeners;

    //==============================================================================
    EditSnapshot (ProjectItemID);
    void refreshFromProjectItem (ProjectItem::Ptr);
    void refreshFromXml (const juce::XmlElement&, const juce::String&, double newLength);
    void refreshFromState();
    void clear();
    void addEditClips (const juce::XmlElement& track);
    void addClipSources (const juce::XmlElement& track);
    void addMarkers (const juce::XmlElement& track);
    void addSubTracksRecursively (const juce::XmlElement& parent, int& audioTrackNameNumber);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSnapshot)
};

} // namespace tracktion_engine
