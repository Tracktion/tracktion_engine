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
/**
    Base class for Clips that produce some kind of audio e.g. WaveAudioClip or
    EditClip. This class contains the common functionality such as gains, fades,
    plugins etc.

    If your source if dynamically generated override needsRender to return true.
    This will then trigger the rest of the render callbacks which should be used
    to generate the source and fill in any update messages etc.

    If your source is static override getAudioFile to return the source file. Note that
    this may be changed by the UI so it is a good idea to also overrride sourceMediaChanged
    and getSourceLength to update properties accordingly when the source ProjectItemID is re-assigned.

    Either way you must override getHash which should return a hash of the current source
    e.g. an AudioFile hash or hash describing the current render set-up of an Edit.
*/
class AudioClipBase    : public Clip,
                         public RenderManager::Job::Listener,
                         private juce::Timer
{
public:
    //==============================================================================
    /** Creates a basic AudioClip. */
    AudioClipBase (const juce::ValueTree&, EditItemID, Type, ClipTrack&);

    /** Destructor. */
    ~AudioClipBase() override;

    //==============================================================================
    /** Returns the maximum length for this clip.
        This can change depending on the clips properties e.g. if the clip is
        timestretched then this will return a proportion of getSourceLength, if it
        is looped this will return infinite.
    */
    TimeDuration getMaximumLength() override;

    /** Must return the length in seconds of the source material e.g. the length
        of the audio file or edit.
    */
    virtual TimeDuration getSourceLength() const = 0;

    /** Returns the file used to play back the source and will get proxied etc. */
    virtual AudioFile getAudioFile() const  { return AudioFile (edit.engine, getCurrentSourceFile()); }

    /** Returns the current AudioFile being used by the Clip, either the
        original source or a proxy.
    */
    AudioFile getPlaybackFile();

    /** Must return the file that the source ProjectItemID refers to. */
    virtual juce::File getOriginalFile() const = 0;

    /** Must return a unique hash for this clip's source.
        This should be the same until the clip changes as it is used to determine if
        the proxy needs regenerating.
    */
    virtual HashCode getHash() const = 0;

    /** Returns the WaveInfo for a clip.
        By default this just looks in the AudioSegmentList cache but subclasses can override
        this to return a custom WaveInfo if they don't reference source files..
    */
    virtual AudioFileInfo getWaveInfo();

    //==============================================================================
    /** Resets the dirty flag so that a new render will be attempted.
        Call this whenever one of your clips properties that will change the source file change.
        This flag is used to avoid constantly re-rendering failed sources.
    */
    void markAsDirty();

    /** Checks the current source file to see if it's up to date and then triggers a source render if needed.
        This uses the needsRender and getHash methods to determine the source file to be used.
        Call this if something changes that will affect the render e.g. tracks in an EditClip.
    */
    void updateSourceFile();

    /** Subclasses should override this to return true if they need the rest of the render callbacks. */
    virtual bool needsRender() const                    { return false; }

    /** Subclasses should override this to return a RenderJob suitable for rendering
        its source file. Note that because we can only render one source this should
        also check to see if the source should be reversed and do so accordingly.
        This will be called on the message thread so should complete quickly, if your source needs some
        time consuming setup (e.g. loading Edits) then make sure your render job does this in its render thread.
    */
    virtual RenderManager::Job::Ptr getRenderJob (const AudioFile&)     { return {}; }

    /** Callback to indicate that the render has completed.
        If you override this, make sure to call the base class implementation first.
    */
    virtual void renderComplete();

    /** Override this to return a custom message to be displayed over waveforms during
        rendering.
        This is called periodically once a render has started. Once you are done, this
        should return an empty string to let the AudioStripBase know to stop updating.
    */
    virtual juce::String getRenderMessage()             { return {}; }

    /** Override this to return a custom message to display over the clip where the "file missing" text usually goes. */
    virtual juce::String getClipMessage()               { return {}; }

    /** If a render is in progress, this will cancel it. */
    void cancelCurrentRender();

    //==============================================================================
    /** Sets the gain of the clip in dB. */
    void setGainDB (float dB);
    /** Returns the gain of the clip in dB. */
    float getGainDB() const noexcept                    { return level->dbGain.get(); }
    /** Returns the gain of the clip. */
    float getGain() const noexcept                      { return dbToGain (level->dbGain.get()); }

    /** Sets the pan of the clip.
        @param pan -1 = full left, 0 = centre, 1 = full right
    */
    void setPan (float pan);
    /** Returns the pan of the clip from -1 to 1 @see setPan. */
    float getPan() const noexcept                       { return level->pan.get(); }

    /** @internal */
    void setMuted (bool shouldBeMuted) override         { level->mute = shouldBeMuted; }
    /** @internal */
    bool isMuted() const override                       { return level->mute.get(); }

    /** Returns a LiveClipLevel which can be used to read the gain, pan and mute statuses. */
    LiveClipLevel getLiveClipLevel();

    //==============================================================================
    /** Enables the left channel of the clip. */
    void setLeftChannelActive (bool);
    /** Returns whether the left channel of the clip is enabled. */
    bool isLeftChannelActive() const;

    /** Enables the right channel of the clip. */
    void setRightChannelActive (bool);
    /** Returns whether the right channel of the clip is enabled. */
    bool isRightChannelActive() const;

    /** Returns the layout of the active channels. */
    juce::AudioChannelSet getActiveChannels() const     { return activeChannels; }

    //==============================================================================
    /** Sets the fade in duration in seconds.
        If the duration is longer than the clip or overlaps the fade out, this will
        reduce the fade out accordingly.
    */
    bool setFadeIn (TimeDuration length);
    /** Returns the fade in duration in seconds. */
    TimeDuration getFadeIn() const;

    /** Sets the fade out duration in seconds.
        If the duration is longer than the clip or overlaps the fade in, this will
        reduce the fade in accordingly.
    */
    bool setFadeOut (TimeDuration length);
    /** Returns the fade out duration in seconds. */
    TimeDuration getFadeOut() const;

    /** Sets the curve shape for the fade in to use. */
    void setFadeInType (AudioFadeCurve::Type);
    /** Returns the curve shape for the fade in to use. */
    AudioFadeCurve::Type getFadeInType() const          { return fadeInType; }

    /** Sets the curve shape for the fade out to use. */
    void setFadeOutType (AudioFadeCurve::Type newType);
    /** Returns the curve shape for the fade out to use. */
    AudioFadeCurve::Type getFadeOutType() const         { return fadeOutType; }

    /** Enables/disables auto-crossfading.
        When enabled, the fade in/out length will automatically be set to the length
        of any overlapping clip regions.
    */
    void setAutoCrossfade (bool shouldAutoCrossfade)    { autoCrossfade = shouldAutoCrossfade; }
    /** Returns whether auto-crossfade is enabled. */
    bool getAutoCrossfade() const noexcept              { return autoCrossfade; }

    /** Triggers an update of the auto-crossfades.
        N.B. you shouldn't normally need to call this, it's called by the
        AudioTrack when clips are moved.
        @param updateOverlapped If true, this will also update any other clips that overlap this one.
    */
    void updateAutoCrossfadesAsync (bool updateOverlapped);
    /** Sets the fade in/out lengths to be 0.03s to avoid any clicks at the start/end of th clip. */
    void applyEdgeFades();

    /** Copies the fade in curve to a volume automation curve.
        @param fadeIn           If true, this copies the fade-in, if false, the fade-out
        @param removeClipFade   If true, the fade in will be set to 0 @see setFadeIn
    */
    void copyFadeToAutomation (bool fadeIn, bool removeClipFade);

    //==============================================================================
    /** Describes the fade behaviour. */
    enum FadeBehaviour
    {
        gainFade = 0,   /**< Fade is a volume/gain ramp. */
        speedRamp       /**< Fade is a change of playback speed for tape start/stop effects. */
    };

    /** Sets the fade in behaviour. */
    void setFadeInBehaviour (FadeBehaviour newBehaviour)    { fadeInBehaviour = newBehaviour; }
    /** Returns the fade in behaviour. */
    FadeBehaviour getFadeInBehaviour() const                { return fadeInBehaviour; }

    /** Sets the fade out behaviour. */
    void setFadeOutBehaviour (FadeBehaviour newBehaviour)   { fadeOutBehaviour = newBehaviour; }
    /** Returns the fade out behaviour. */
    FadeBehaviour getFadeOutBehaviour() const               { return fadeOutBehaviour; }

    //==============================================================================
    /** Override this to fill in the LoopInfo structure as best fits the source. */
    virtual void setLoopDefaults() = 0;

    /** Sets a LoopInfo to describe this clip's tempo, time sig etc. which is used
        when syncing to the TempoSequence and PitchSequence.
        @see LoopInfo. */
    void setLoopInfo (const LoopInfo&);
    /** Returns the LoopInfo being used to describe this clip. */
    const LoopInfo& getLoopInfo() const                 { return loopInfo; }
    /** Returns the LoopInfo being used to describe this clip. */
    LoopInfo& getLoopInfo()                             { return loopInfo; }

    /** Returns the loop range in seconds. */
    TimeRange getLoopRange() const;

    /** @internal */
    bool canLoop() const override                       { return ! isUsingMelodyne(); }
    /** @internal */
    bool isLooping() const override                     { return getAutoTempo() ? (loopLengthBeats > BeatDuration()) : (loopLength > TimeDuration()); }
    /** @internal */
    bool beatBasedLooping() const override              { return isLooping() && getAutoTempo(); }
    /** @internal */
    void setNumberOfLoops (int num) override;
    /** @internal */
    void disableLooping() override;
    /** @internal */
    BeatPosition getLoopStartBeats() const override;
    /** @internal */
    TimePosition getLoopStart() const override;
    /** @internal */
    BeatDuration getLoopLengthBeats() const override;
    /** @internal */
    TimeDuration getLoopLength() const override;
    /** @internal */
    void setLoopRange (TimeRange) override;
    /** @internal */
    void setLoopRangeBeats (BeatRange) override;

    /** Enables auto-detection of beats.
        If this is true the LoopInfo will be set based on what beats were detected.
        This will aso updat eif the sensitivity is changed @see setBeatSensitivity.
        [[ blocks ]]
    */
    void setAutoDetectBeats (bool);
    /** Returns true if auto-detect of beats is enabled. */
    bool getAutoDetectBeats() const                     { return autoDetectBeats; }

    /** Sets the beat sensitivity, triggering a LoopInfo update if auto-detect is enabled.
        @see setAutoDetectBeats
    */
    void setBeatSensitivity (float s);
    /** Returns the beat sensitivity. */
    float getBeatSensitivity() const                    { return beatSensitivity; }

    /** @internal */
    void pitchTempoTrackChanged() override;

    //==============================================================================
    /** @internal */
    void setSpeedRatio (double newSpeed) override;
    
    /** Sets a time-stretch mode to use. */
    void setTimeStretchMode (TimeStretcher::Mode mode);
    
    /** Returns the time-stretch mode that has been set. */
    TimeStretcher::Mode getTimeStretchMode() const noexcept;
    
    /** Returns the time-stretch mode that is in use.
        Note that even if not time-stretch mode has been set e.g. for speed changes,
        if auto-pitch or auto-tempo is enabled, a time-stretch mode will have to be
        used and this returns it.
    */
    TimeStretcher::Mode getActualTimeStretchMode() const noexcept;

    //==============================================================================
    /** Defines the auto pitch mode. */
    enum AutoPitchMode
    {
        pitchTrack = 0, /**< Clip tracks the Edit's PitchSequence. */
        chordTrackMono, /**< Clip tracks the chord track with a monophonic pitch change. */
        chordTrackPoly  /**< Clip tracks the pitch track with a polyphonic pitch change. */
    };

    /** Enables/disables auto-tempo.
        If enabled, this clip will adjust its playback speed to stay in sync with the Edit's TempoSequence.
    */
    void setAutoTempo (bool shouldUseAutoTempo)         { autoTempo = shouldUseAutoTempo; }
    
    /** Returns true if auto-tempo has been set. */
    bool getAutoTempo() const                           { return autoTempo; }

    /** Enables/disables auto-pitch.
        If enabled, this clip will adjust its playback pitch to stay in sync with the Edit's PitchSequence.
    */
    void setAutoPitch (bool shouldUseAutoPitch)         { autoPitch = shouldUseAutoPitch; }

    /** Returns true if auto-pitch has been set. */
    bool getAutoPitch() const                           { return autoPitch; }

    /** Sets the AutoPitchMode to use. */
    void setAutoPitchMode (AutoPitchMode m)             { autoPitchMode = m; }
    
    /** Returns the AutoPitchMode in use. */
    AutoPitchMode getAutoPitchMode()                    { return autoPitchMode; }
    
    /** Enables/disables warp time.
        Warp Time enables segmented warping of the audio. @see WarpTimeManager
    */
    void setWarpTime (bool shouldUseWarpTime)           { warpTime = shouldUseWarpTime; }
    
    /** Returns true if warp time is enabled. */
    bool getWarpTime() const                            { return warpTime; }

    /** Returns the WarpTimeManager for this clip used to maipluate warp markers. */
    WarpTimeManager& getWarpTimeManager() const;

    /** Sets the number of semitones to transpose the clip by. */
    void setTranspose (int numSemitones)                { transpose = juce::jlimit (-24, 24, numSemitones); }

    /** Returns the number of semitones this clip will be changed by.
        N.B. this is used if auto-pitch is enabled.
        @param includeAutoPitch If true, this will include the changes defined by auto-pitch.
                                If false, this will only be the number explicitly set in setTranspose
    */
    int getTransposeSemiTones (bool includeAutoPitch) const;

    /** Sets the number of semitones to transpose the clip by.
        N.B. this is only used if auto-pitch is disabled.
    */
    void setPitchChange (float semitones)               { pitchChange = juce::jlimit (-48.0f, 48.0f, semitones); }
    
    /** Returns the number of semitones to transpose the clip by. */
    float getPitchChange() const                        { return pitchChange; }
    
    /** Returns the pitch change as a normalised ratio. 0 = no change, <0 = pitched down, >0 = pitched up. */
    float getPitchRatio() const                         { return juce::jlimit (0.1f, 10.0f, std::pow (2.0f, pitchChange / 12.0f)); }

    /** Enables reversing of the clip's source material. */
    void setIsReversed (bool shouldBeReversed)          { isReversed = shouldBeReversed; }

    /** Returns true if the clip's source material is reversed. */
    bool getIsReversed() const noexcept                 { return isReversed; }

    /** Scans the current source file for any beats and adds them to the LoopInfo returned.
        @param current      A LoopInfo to initialise the result from
        @param autoBeat     Whether to auto detect the beats from the source file
        @param sensitivity  The sensitivity to use 0-1
        [[ blocks ]]
    */
    LoopInfo autoDetectBeatMarkers (const LoopInfo& current, bool autoBeat, float sensitivity) const;
    
    /** Performs a tempo-detection task and if successful sets the clip's LoopInfo tempo to this.
        @returns true if the tempo was sensibly detected
        [[ blocks ]]
    */
    bool performTempoDetect();

    /** Returns an array of the root note choices e.g. "C#" etc. */
    static juce::StringArray getRootNoteChoices (Engine&);

    /** Returns an array describng what pitch each semitone change will be. */
    juce::StringArray getPitchChoices();

    //==============================================================================
    /** Returns true if this clip can have ClipEffects added to it. */
    virtual bool canHaveEffects() const                 { return ! (warpTime || isReversed); }

    /** Enables/disables ClipEffects for this clip.
        @param enable   Whether to turn on/off clip FX for this clip
        @param warn     If true and clip FX are enabled, this will show a confirmation dialog to the user first
    */
    void enableEffects (bool enable, bool warn);
    
    /** Returns true if ClipEffects are enabled. */
    bool effectsEnabled() const                         { return clipEffects != nullptr; }

    /** Adds a ClipEffect to this clip. */
    void addEffect (const juce::ValueTree& effectsTree);

    /** Sets the effectsVisible flag for this clip. */
    void setEffectsVisible (bool b)                     { clipEffectsVisible = b; }
    
    /** Returns true if the effectsVisible flag is set for this clip. */
    bool getEffectsVisible() const                      { return clipEffectsVisible; }

    /** Returns the ClipEffects for this clip if it has been enabled. */
    ClipEffects* getClipEffects() const noexcept        { return clipEffects.get(); }

    //==============================================================================
    /** Returns true if source file has a bwav time reference metadata property. */
    bool canSnapToOriginalBWavTime();
    
    /** Moves the clip to the bwav time reference metadata property time. */
    void snapToOriginalBWavTime();

    //==============================================================================
    /** Returns the ProjectItemID of the clip's takes. */
    virtual juce::Array<ProjectItemID> getTakes() const;

    //==============================================================================
    /** Returns an empty string if this plugin can be added, otherwise an error message
        due to the clip plugin being an incorrect type (e.g. MIDI) or the list is full.
    */
    juce::String canAddClipPlugin (const Plugin::Ptr&) const;

    //==============================================================================
    /** Holds information about how to render a proxy for this clip. */
    struct ProxyRenderingInfo
    {
        /** Constructor. */
        ProxyRenderingInfo();
        /** Destructor. */
        ~ProxyRenderingInfo();

        std::unique_ptr<AudioSegmentList> audioSegmentList;
        TimeRange clipTime;
        double speedRatio;
        TimeStretcher::Mode mode;
        TimeStretcher::ElastiqueProOptions options;

        /** Renders this audio segment list to an AudioFile. */
        bool render (Engine&, const AudioFile&, AudioFileWriter&, juce::ThreadPoolJob* const&, std::atomic<float>& progress) const;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProxyRenderingInfo)
    };

    /** Should return true if the clip is referencing the file in any way. */
    virtual bool isUsingFile (const AudioFile&);

    /** Returns the AudioFile to create to play this clip back.
        @param renderTimestretched If true, this should be a time-stretched version of the clip @see setAutoTempo
    */
    AudioFile getProxyFileToCreate (bool renderTimestretched);
    
    /** Can be used to disable proxy file generation for this clip.
        If disabled, the audio engine will time-stretch the file in real time which may use more CPU.
    */
    void setUsesProxy (bool canUseProxy) noexcept;
    
    /** Retuns true if this clip can use a proxy file. */
    bool canUseProxy() const noexcept               { return proxyAllowed && edit.canRenderProxies(); }

    /** Retuns true if this clip use a proxy file due to timestretching.
        This can be because auto-tempo, auto-pitch or a pitch change has been set.
    */
    bool usesTimeStretchedProxy() const;
    
    /** Creates a ProxyRenderingInfo object to decribe the stretch segements of this clip. */
    std::unique_ptr<ProxyRenderingInfo> createProxyRenderingInfo();

    /** Returns a hash identifying the proxy settings. */
    HashCode getProxyHash();

    /** Triggers creation of a new proxy file if one is required. */
    void beginRenderingNewProxyIfNeeded();

    /** Can be enabled to use a simpler playback node for time-stretched previews. */
    void setUsesTimestretchedPreview (bool shouldUsePreview) noexcept   { useTimestretchedPreview = shouldUsePreview; }

    /** Returns true if this clp should use a time-stretched preview. */
    bool usesTimestretchedPreview() const noexcept                      { return useTimestretchedPreview; }

    /** Returns an AudioSegmentList describing this file if it is using auto-tempo.
        This can be useful for drawing waveforms.
        [[ message_thread ]]
    */
    const AudioSegmentList& getAudioSegmentList();

    /** Sets the resampling qulity to use.
        This is only applicable if setUsesProxy has been set to false.
        If a proxy is used, Lagrange interpolation will be used.
        N.B. the higher the quality, the more higher the CPU usage during playback.
    */
    void setResamplingQuality (ResamplingQuality);

    /** Returns the resampling quality to the be used. */
    ResamplingQuality getResamplingQuality() const;

    //==============================================================================
    /** Reverses the loop points to expose the same section of the source file but reversed. */
    void reverseLoopPoints();
    
    /** Trims the fade in out lengths to avoid any overlap between them. */
    void checkFadeLengthsForOverrun();

    /** Defines a prevous/next direction. @see getOverlappingClip */
    enum class ClipDirection { previous, next, none };
    
    /** Returns the previous/next overlapping clip if one exists. */
    AudioClipBase* getOverlappingClip (ClipDirection) const;

    //==============================================================================
    /** The MelodyneFileReader proxy if this clip is using Melodyne. */
    juce::ReferenceCountedObjectPtr<MelodyneFileReader> melodyneProxy;

    /** Returns true if this clip is using Melodyne. */
    bool isUsingMelodyne() const;

    /** Shows the Melodyne window if this clip is using Melodyne. */
    void showMelodyneWindow();

    /** Hides the Melodyne window if this clip is using Melodyne. */
    void hideMelodyneWindow();

    /** If this clip is using Melodyne, this will create a new MIDI clip based
        on the Melodyne analysis.
    */
    void melodyneConvertToMIDI();

    /** @internal */
    void loadMelodyneState();

    /** This internal method is used solely to find out if createAudioNode()
        should return nullptr or not.

        Though a bit spaghetti, the ARA node generation will be handled elsewhere,
        the parent AudioTrack, for the sake of live-play.

        @returns False if something went wrong!

                 It's possible no ARA-compatible plugins were found,
                 or that ARA complained about something resulting
                 in failure to set it up accordingly.
                 Celemony's ARA is really flaky and touchy, so the latter
                 is most likely!
    */
    bool setupARA (bool dontPopupErrorMessages);

    /** The ElastiqueProOptions for fine tuning Elastique (if available). */
    juce::CachedValue<TimeStretcher::ElastiqueProOptions> elastiqueProOptions;

    //==============================================================================
    /** @internal */
    void initialise() override;
    /** @internal */
    void cloneFrom (Clip*) override;
    /** @internal */
    void flushStateToValueTree() override;

    //==============================================================================
    /** @internal */
    void setTrack (ClipTrack*) override;
    /** @internal */
    bool canGoOnTrack (Track&) override;
    /** @internal */
    void changed() override;

    //==============================================================================
    /** @internal */
    juce::Colour getDefaultColour() const override;
    /** @internal */
    PatternGenerator* getPatternGenerator() override;

    //==============================================================================
    /** @internal */
    void addMark (TimePosition relCursorPos);
    /** @internal */
    void moveMarkTo (TimePosition relCursorPos);
    /** @internal */
    void deleteMark (TimePosition relCursorPos);
    /** @internal */
    void getRescaledMarkPoints (juce::Array<TimePosition>& rescaled, juce::Array<int>& orig) const;
    /** @internal */
    juce::Array<TimePosition> getRescaledMarkPoints() const override;

    /** @internal */
    juce::Array<ReferencedItem> getReferencedItems() override;
    /** @internal */
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

    //==============================================================================
    /** @internal */
    bool addClipPlugin (const Plugin::Ptr&, SelectionManager&) override;
    /** @internal */
    Plugin::Array getAllPlugins() override;
    /** @internal */
    void sendMirrorUpdateToAllPlugins (Plugin&) const override;
    /** @internal */
    PluginList* getPluginList() override           { return &pluginList; }

protected:
    //==============================================================================
    friend class WaveCompManager;

    //==============================================================================
    std::shared_ptr<ClipLevel> level { std::make_shared<ClipLevel>() };
    juce::CachedValue<juce::String> channels;

    juce::CachedValue<TimeDuration> fadeIn, fadeOut;
    TimeDuration autoFadeIn, autoFadeOut;
    juce::CachedValue<AudioFadeCurve::Type> fadeInType, fadeOutType;
    juce::CachedValue<bool> autoCrossfade;
    juce::CachedValue<FadeBehaviour> fadeInBehaviour, fadeOutBehaviour;
    juce::CachedValue<ResamplingQuality> resamplingQuality;

    juce::CachedValue<TimePosition> loopStart;
    juce::CachedValue<TimeDuration> loopLength;
    juce::CachedValue<BeatPosition> loopStartBeats;
    juce::CachedValue<BeatDuration> loopLengthBeats;

    juce::CachedValue<int> proxyAllowed;
    juce::CachedValue<int> transpose;
    juce::CachedValue<float> pitchChange;
    LoopInfo loopInfo;

    juce::CachedValue<float> beatSensitivity;
    juce::CachedValue<TimeStretcher::Mode> timeStretchMode;
    juce::CachedValue<bool> autoPitch, autoTempo, isReversed, autoDetectBeats, warpTime, clipEffectsVisible;
    juce::CachedValue<AutoPitchMode> autoPitchMode;

    mutable WarpTimeManager::Ptr warpTimeManager;
    mutable std::unique_ptr<AudioSegmentList> audioSegmentList;
    std::unique_ptr<ClipEffects> clipEffects;
    AsyncFunctionCaller asyncFunctionCaller;

    juce::AudioChannelSet activeChannels;
    void updateLeftRightChannelActivenessFlags();

    bool useTimestretchedPreview = false;
    PluginList pluginList;

    bool lastRenderJobFailed = false;

    RenderManager::Job::Ptr renderJob;
    AudioFile lastProxy;

    //==============================================================================
    /** Triggers a source or proxy render after a timeout. Call this if something changes that
        may affect either the source or the proxy e.g. Edit clip tracks or tempo sequence.
    */
    void createNewProxyAsync();

    /** @internal */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    /** @internal */
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override;
    /** @internal */
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override;
    /** @internal */
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    /** @internal */
    void valueTreeParentChanged (juce::ValueTree&) override;

private:
    //==============================================================================
    class TempoDetectTask;
    class BeatSensitivityComp;

    void updateReversedState();
    void updateAutoTempoState();
    void updateClipEffectsState();

    enum { updateCrossfadesFlag = 0, updateCrossfadesOverlappedFlag = 1 };
    void updateAutoCrossfades (bool updateOverlapped);

    /** Checks the source's hash to see if it has changed and if so starts a new render
        process.This will in turn call getRenderJob on a background thead.
    */
    void renderSource();
    bool shouldAttemptRender() const    { return (! lastRenderJobFailed) && needsRender(); }

    void clearCachedAudioSegmentList();

    //==============================================================================
    void jobFinished (RenderManager::Job& job, bool completedOk) override;
    void timerCallback() override;

    TimePosition clipTimeToSourceFileTime (TimePosition clipTime);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipBase)
};

}} // namespace tracktion { inline namespace engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion::engine::AudioClipBase::FadeBehaviour>
    {
        static tracktion::engine::AudioClipBase::FadeBehaviour fromVar (const var& v)   { return (tracktion::engine::AudioClipBase::FadeBehaviour) static_cast<int> (v); }
        static var toVar (tracktion::engine::AudioClipBase::FadeBehaviour v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion::engine::AudioClipBase::AutoPitchMode>
    {
        static tracktion::engine::AudioClipBase::AutoPitchMode fromVar (const var& v)   { return (tracktion::engine::AudioClipBase::AutoPitchMode) static_cast<int> (v); }
        static var toVar (tracktion::engine::AudioClipBase::AutoPitchMode v)            { return static_cast<int> (v); }
    };
}
