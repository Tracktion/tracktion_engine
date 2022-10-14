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
class AudioTrack  : public ClipTrack,
                    private juce::Timer
{
public:
    AudioTrack (Edit&, const juce::ValueTree&);
    ~AudioTrack() override;

    using Ptr = juce::ReferenceCountedObjectPtr<AudioTrack>;

    void initialise() override;
    bool isAudioTrack() const override                      { return true; }

    //==============================================================================
    juce::String getSelectableDescription() override        { return TRANS("Track") + " - \"" + getName() + "\""; }

    //==============================================================================
    // This isn't an index - it starts from 1
    int getAudioTrackNumber() noexcept;

    /** checks whether the name is 'track n' and if so, makes sure the number is right */
    void sanityCheckName() override;
    juce::String getName() override;

    bool canContainPlugin (Plugin*) const override;
    bool processAudioNodesWhileMuted() const override       { return edit.engine.getEngineBehaviour().shouldProcessMutedTracks() || isSidechainSource(); }

    //==============================================================================
    /** returns a warning message about this track not being playable, or "" if it's ok */
    juce::String getTrackPlayabilityWarning() const;

    //==============================================================================
    VolumeAndPanPlugin* getVolumePlugin();
    LevelMeterPlugin* getLevelMeterPlugin();
    EqualiserPlugin* getEqualiserPlugin();
    AuxSendPlugin* getAuxSendPlugin (int bus = -1) const; // -1 == any bus, first aux found

    //==============================================================================
    /** looks for a name for a midi note by trying all the plugins, and returning a
        default on failure. midiChannel is 1-16
    */
    juce::String getNameForMidiNoteNumber (int note, int midiChannel, bool preferSharp = true) const;

    /** prog number is 0 based. */
    juce::String getNameForProgramNumber (int programNumber, int bank) const;
    juce::String getNameForBank (int bank) const;
    int getIdForBank (int bank) const;
    bool areMidiPatchesZeroBased() const;

    //==============================================================================
    WaveInputDevice& getWaveInputDevice() const noexcept        { jassert (waveInputDevice); return *waveInputDevice; }
    MidiInputDevice& getMidiInputDevice() const noexcept        { jassert (midiInputDevice); return *midiInputDevice; }

    TrackOutput& getOutput() const noexcept                     { return *output; }

    int getMaxNumOfInputs() const noexcept                      { return maxInputs; }
    void setMaxNumOfInputs (int newMaxNumber);

    /** checks whether audio clips can be played - i.e. can they make it past
        the plugins, and is the track going to an audio output */
    bool canPlayAudio() const;
    bool canPlayMidi() const;

    //==============================================================================
    bool isFrozen (FreezeType) const override;
    void setFrozen (bool, FreezeType) override;
    void insertFreezePointAfterPlugin (const Plugin::Ptr&);
    void removeFreezePoint();
    void freezeTrackAsync() const;

    //==============================================================================
    bool hasAnyLiveInputs();
    bool hasAnyTracksFeedingIn();

    juce::Array<Track*> getInputTracks() const override;
    juce::Array<Track*> findSidechainSourceTracks() const;

    void injectLiveMidiMessage (const MidiMessageArray::MidiMessageWithSource&);
    void injectLiveMidiMessage (const juce::MidiMessage&, MidiMessageArray::MPESourceID);

    //==============================================================================
    bool isMuted (bool includeMutingByDestination) const override;
    bool isSolo (bool includeIndirectSolo) const override;
    bool isSoloIsolate (bool includeIndirectSolo) const override;

    void setMute (bool) override;
    void setSolo (bool) override;
    void setSoloIsolate (bool) override;

    //==============================================================================
    /** vertical scales for displaying the midi note editor */
    double getMidiVisibleProportion() const                         { return midiVisibleProportion; }
    double getMidiVerticalOffset() const                            { return midiVerticalOffset; }
    void setMidiVerticalPos (double visibleProp, double offset);
    void scaleVerticallyToFitMidi();
    void setVerticalScaleToDefault();

    void setTrackToGhost (AudioTrack*, bool shouldGhost);
    void clearGhostTracks()                                         { ghostTracks.resetToDefault(); }
    juce::Array<AudioTrack*> getGhostTracks() const;

    int getCompGroup() const noexcept                               { return compGroup.get(); }
    void setCompGroup (int groupIndex)                              { compGroup = groupIndex; }

    //==============================================================================
    bool hasMidiNoteNames()                                         { return midiNoteMapCache.size() > 0; }
    void clearMidiNoteNames()                                       { midiNoteMap = juce::String(); }
    void loadMidiNoteNames (const juce::String namesFile)           { midiNoteMap = namesFile; }

    //==============================================================================
    /** try to add this MIDI sequence to any MIDI clips that are already in the track.

        @returns False if there's no existing clips in the places it needs them.
    */
    bool mergeInMidiSequence (const juce::MidiMessageSequence&, TimePosition startTime,
                              MidiClip*, MidiList::NoteAutomationType);

    void playGuideNote (int note, MidiChannel midiChannel, int velocity,
                        bool stopOtherFirst = true, bool forceNote = false, bool autorelease = false);

    void playGuideNotes (const juce::Array<int>& notes, MidiChannel midiChannel,
                         const juce::Array<int>& velocities,
                         bool stopOthersFirst = true);

    void turnOffGuideNotes();
    void turnOffGuideNotes (MidiChannel);

    //==============================================================================
    /** Prevents the freeze point from being removed during an unfreeze operation. */
    struct FreezePointRemovalInhibitor
    {
        FreezePointRemovalInhibitor (AudioTrack&);
        ~FreezePointRemovalInhibitor();
        AudioTrack& track;
    };

    //==============================================================================
    /** Listener interface to be notified of recorded MIDI being sent to the plugins. */
    struct Listener
    {
        /* Destructor */
        virtual ~Listener() = default;

        /** Called when a MidiMessage (i.e. from a soft-keyboard) should be injected in to the playback graph.
            If the message was used, the listener should set the wasUsed argument to true or a
            message may be shown to the user to notify them of why they couldn't hear the sound.
        */
        virtual void injectLiveMidiMessage (AudioTrack&, const MidiMessageArray::MidiMessageWithSource&, bool& wasUsed) = 0;

        /** Called when a recorded MidiMessage (i.e. from a clip) has been sent to the plugin chain.
            This will be called from the message thread.
        */
        virtual void recordedMidiMessageSentToPlugins (AudioTrack&, const juce::MidiMessage&) = 0;
    };

    /** Adds a Listener.
        This will automatically call Edit::restartPlayback for the callbacks to take effect.
    */
    void addListener (Listener*);

    /** Removes a Listener. */
    void removeListener (Listener*);
    
    /** Returns the listener list so Nodes can manually call them. */
    juce::ListenerList<Listener>& getListeners()            { return listeners; }

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    bool isTrackAudible (bool areAnyTracksSolo) const override;

    void timerCallback() override   { turnOffGuideNotes(); }

private:
    //==============================================================================
    struct TrackMuter;
    struct FreezeUpdater;
    friend struct TrackMuter;
    friend class Edit;
    friend class Clip;

    //==============================================================================
    std::unique_ptr<TrackOutput> output;
    std::unique_ptr<WaveInputDevice> waveInputDevice;
    std::unique_ptr<MidiInputDevice> midiInputDevice;

    juce::CachedValue<bool> muted, soloed, soloIsolated;
    juce::CachedValue<bool> frozen, frozenIndividually;

    int freezePointRemovalInhibitor = 0;
    juce::CachedValue<int> maxInputs, compGroup;

    juce::CachedValue<double> midiVisibleProportion, midiVerticalOffset;
    juce::CachedValue<juce::String> ghostTracks;
    juce::CachedValue<juce::String> midiNoteMap;

    std::map<int, juce::String> midiNoteMapCache;

    juce::Array<int> currentlyPlayingGuideNotes;

    std::unique_ptr<FreezeUpdater> freezeUpdater;
    std::unique_ptr<TrackMuter> trackMuter;

    enum { updateAutoCrossfadesFlag = 1 };
    AsyncFunctionCaller asyncCaller;

    juce::ListenerList<Listener> listeners;

    //==============================================================================
    void freezeTrack();
    bool insertFreezePointIfRequired();
    int getIndexOfDefaultFreezePoint();
    int getIndexOfFreezePoint();
    void freezePlugins (juce::Range<int> pluginsToFreeze);
    void unFreezeTrack();
    juce::File getFreezeFile() const;

    void updateTracksToGhost();
    bool isSidechainSource() const;

    void updateMidiNoteMapCache();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrack)
};

}} // namespace tracktion { inline namespace engine
