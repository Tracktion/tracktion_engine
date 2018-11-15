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
class AudioTrack  : public ClipTrack
{
public:
    AudioTrack (Edit&, const juce::ValueTree&);
    ~AudioTrack();

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
    bool processAudioNodesWhileMuted() const override       { return isSidechainSource(); }

    //==============================================================================
    /** returns a warning message about this track not being playable, or "" if it's ok */
    juce::String getTrackPlayabilityWarning() const;

    //==============================================================================
    VolumeAndPanPlugin* getVolumePlugin();
    LevelMeterPlugin* getLevelMeterPlugin();
    EqualiserPlugin* getEqualiserPlugin();
    AuxSendPlugin* getAuxSendPlugin (int bus = -1); // -1 == any bus, first aux found

    //==============================================================================
    /** looks for a name for a midi note by trying all the plugins, and returning a
        default on failure. midiChannel is 1-16
    */
    juce::String getNameForMidiNoteNumber (int note, int midiChannel) const;

    /** prog number is 0 based. */
    juce::String getNameForProgramNumber (int programNumber, int bank) const;
    juce::String getNameForBank (int bank) const;
    int getIdForBank (int bank) const;
    bool areMidiPatchesZeroBased() const;

    //==============================================================================
    WaveInputDevice& getWaveInputDevice() const noexcept        { return *waveInputDevice; }
    MidiInputDevice& getMidiInputDevice() const noexcept        { return *midiInputDevice; }

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
    static juce::String getTrackFreezePrefix() noexcept             { return "trackFreeze_"; }
    void freezeTrackAsync() const;

    //==============================================================================
    /** creates an audio node to play this track.
        (only creates one if this track doesn't feed into another track)
    */
    AudioNode* createAudioNode (const CreateAudioNodeParams&);

    bool hasAnyLiveInputs();
    bool hasAnyTracksFeedingIn();

    juce::Array<AudioTrack*> getInputTracks() const;
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
    /** try to add this MIDI sequence to any MIDI clips that are already in the track.

        @returns False if there's no existing clips in the places it needs them.
    */
    bool mergeInMidiSequence (const juce::MidiMessageSequence&, double startTime,
                              MidiClip*, MidiList::NoteAutomationType);

    void playGuideNote (int note, MidiChannel midiChannel, int velocity,
                        bool stopOtherFirst = true, bool forceNote = false);

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

        /** Called when a recorded MidiMessage (i.e. from a clip) has been sent to the plugin chain.
            This will be called from the message thread.
        */
        virtual void recordedMidiMessageSentToPlugins (AudioTrack&, const juce::MidiMessage&) = 0;
    };

    /** Adds a Listener. */
    void addListener (Listener*);

    /** Removes a Listener. */
    void removeListener (Listener*);

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    bool isTrackAudible (bool areAnyTracksSolo) const override;

private:
    //==============================================================================
    struct TrackMuter;
    struct FreezeUpdater;
    struct LiveMidiOutputAudioNode;
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

    juce::Array<int> currentlyPlayingGuideNotes;

    std::unique_ptr<FreezeUpdater> freezeUpdater;
    std::unique_ptr<TrackMuter> trackMuter;

    enum { updateAutoCrossfadesFlag = 1 };
    AsyncFunctionCaller asyncCaller;

    juce::ListenerList<Listener> listeners;

    //==============================================================================
    bool canUseBufferedAudioNode();

    void freezeTrack();
    bool insertFreezePointIfRequired();
    int getIndexOfDefaultFreezePoint();
    int getIndexOfFreezePoint();
    void freezePlugins (juce::Range<int> pluginsToFreeze);
    void unFreezeTrack();
    juce::File getFreezeFile() const noexcept;
    AudioNode* createFreezeAudioNode (bool addAntiDenormalisationNoise);

    void updateTracksToGhost();
    bool isSidechainSource() const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrack)
};

} // namespace tracktion_engine
