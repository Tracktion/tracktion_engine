/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
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
    double getMaximumLength() override;

    /** Must return the length in seconds of the source material e.g. the length
        of the audio file or edit.
    */
    virtual double getSourceLength() const = 0;

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
    virtual juce::int64 getHash() const = 0;

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
    float getGainDB() const noexcept                    { return level->dbGain; }
    /** Returns the gain of the clip. */
    float getGain() const noexcept                      { return dbToGain (level->dbGain); }

    /** Sets the pan of the clip.
        @param pan -1 = full left, 0 = centre, 1 = full right
    */
    void setPan (float pan);
    /** Returns the pan of the clip from -1 to 1 @see setPan. */
    float getPan() const noexcept                       { return level->pan; }

    /** @internal */
    void setMuted (bool shouldBeMuted) override         { level->mute = shouldBeMuted; }
    /** @internal */
    bool isMuted() const override                       { return level->mute; }

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
    bool setFadeIn (double length);
    /** Returns the fade in duration in seconds. */
    double getFadeIn() const;

    /** Sets the fade out duration in seconds.
        If the duration is longer than the clip or overlaps the fade in, this will
        reduce the fade in accordingly.
    */
    bool setFadeOut (double length);
    /** Returns the fade out duration in seconds. */
    double getFadeOut() const;

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
    EditTimeRange getLoopRange() const;

    /** @internal */
    bool canLoop() const override                       { return ! isUsingMelodyne(); }
    /** @internal */
    bool isLooping() const override                     { return getAutoTempo() ? (loopLengthBeats > 0.0) : (loopLength > 0.0); }
    /** @internal */
    bool beatBasedLooping() const override              { return isLooping() && getAutoTempo(); }
    /** @internal */
    void setNumberOfLoops (int num) override;
    /** @internal */
    void disableLooping() override;
    /** @internal */
    double getLoopStartBeats() const override;
    /** @internal */
    double getLoopStart() const override;
    /** @internal */
    double getLoopLengthBeats() const override;
    /** @internal */
    double getLoopLength() const override;
    /** @internal */
    void setLoopRange (EditTimeRange) override;
    /** @internal */
    void setLoopRangeBeats (juce::Range<double>) override;

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
    virtual bool canHaveEffects() const                 { return ! (warpTime || isReversed); }
    void enableEffects (bool, bool warn);
    bool effectsEnabled() const                         { return clipEffects != nullptr; }

    void addEffect (const juce::ValueTree& effectsTree);

    void setEffectsVisible (bool b)                     { clipEffectsVisible = b; }
    bool getEffectsVisible() const                      { return clipEffectsVisible; }

    ClipEffects* getClipEffects() const noexcept        { return clipEffects.get(); }

    //==============================================================================
    void addMark (double relCursorPos);
    void moveMarkTo (double relCursorPos);
    void deleteMark (double relCursorPos);

    juce::Array<double> getRescaledMarkPoints() const override;

    //==============================================================================
    bool canSnapToOriginalBWavTime();
    void snapToOriginalBWavTime();

    //==============================================================================
    juce::Array<ReferencedItem> getReferencedItems() override;
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

    virtual juce::Array<ProjectItemID> getTakes() const;

    //==============================================================================
    juce::String canAddClipPlugin (const Plugin::Ptr&) const;
    bool addClipPlugin (const Plugin::Ptr&, SelectionManager&) override;
    Plugin::Array getAllPlugins() override;
    void sendMirrorUpdateToAllPlugins (Plugin&) const override;

    PluginList* getPluginList() override           { return &pluginList; }

    //==============================================================================
    /** Holds information about how to render a proxy for this clip. */
    struct ProxyRenderingInfo
    {
        ProxyRenderingInfo();
        ~ProxyRenderingInfo();

        std::unique_ptr<AudioSegmentList> audioSegmentList;
        EditTimeRange clipTime;
        double speedRatio;
        TimeStretcher::Mode mode;
        TimeStretcher::ElastiqueProOptions options;

        bool render (Engine&, const AudioFile&, AudioFileWriter&, juce::ThreadPoolJob* const&, std::atomic<float>& progress) const;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProxyRenderingInfo)
    };

    /** Should return true if the clip is referencing the file in any way. */
    virtual bool isUsingFile (const AudioFile&);

    AudioFile getProxyFileToCreate (bool renderTimestretched);
    void setUsesProxy (bool canUseProxy) noexcept;
    bool canUseProxy() const noexcept               { return proxyAllowed && edit.canRenderProxies(); }
    bool usesTimeStretchedProxy() const;
    std::unique_ptr<ProxyRenderingInfo> createProxyRenderingInfo();

    juce::int64 getProxyHash();
    void beginRenderingNewProxyIfNeeded();

    void setUsesTimestretchedPreview (bool shouldUsePreview) noexcept   { useTimestretchedPreview = shouldUsePreview; }
    bool usesTimestretchedPreview() const noexcept                      { return useTimestretchedPreview; }

    void reverseLoopPoints();
    void checkFadeLengthsForOverrun();

    enum class ClipDirection { previous, next, none };
    AudioClipBase* getOverlappingClip (ClipDirection) const;

    void getRescaledMarkPoints (juce::Array<double>& rescaled, juce::Array<int>& orig) const;

    juce::ReferenceCountedObjectPtr<MelodyneFileReader> melodyneProxy;

    bool isUsingMelodyne() const;
    void loadMelodyneState();
    void showMelodyneWindow();
    void hideMelodyneWindow();
    void melodyneConvertToMIDI();

    juce::CachedValue<TimeStretcher::ElastiqueProOptions> elastiqueProOptions;

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


protected:
    //==============================================================================
    friend class WaveCompManager;

    //==============================================================================
    std::shared_ptr<ClipLevel> level { std::make_shared<ClipLevel>() };
    juce::CachedValue<juce::String> channels;

    juce::CachedValue<double> fadeIn, fadeOut;
    double autoFadeIn = 0, autoFadeOut = 0;
    juce::CachedValue<AudioFadeCurve::Type> fadeInType, fadeOutType;
    juce::CachedValue<bool> autoCrossfade;
    juce::CachedValue<FadeBehaviour> fadeInBehaviour, fadeOutBehaviour;

    juce::CachedValue<double> loopStart, loopLength;
    juce::CachedValue<double> loopStartBeats, loopLengthBeats;

    juce::CachedValue<int> transpose;
    juce::CachedValue<float> pitchChange;
    LoopInfo loopInfo;

    juce::CachedValue<float> beatSensitivity;
    juce::CachedValue<TimeStretcher::Mode> timeStretchMode;
    juce::CachedValue<bool> autoPitch, autoTempo, isReversed, autoDetectBeats, warpTime, clipEffectsVisible;
    juce::CachedValue<AutoPitchMode> autoPitchMode;

    mutable WarpTimeManager::Ptr warpTimeManager;
    std::unique_ptr<AudioSegmentList> audioSegmentList;
    std::unique_ptr<ClipEffects> clipEffects;
    AsyncFunctionCaller asyncFunctionCaller;

    juce::AudioChannelSet activeChannels;
    void updateLeftRightChannelActivenessFlags();

    bool proxyAllowed = true, useTimestretchedPreview = false;
    PluginList pluginList;

    bool lastRenderJobFailed = false;

    RenderManager::Job::Ptr renderJob;
    AudioFile lastProxy;

    //==============================================================================
    /** Triggers a source or proxy render after a timeout. Call this if something changes that
        may affect either the source or the proxy e.g. Edit clip tracks or tempo sequence.
    */
    void createNewProxyAsync();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
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
    const AudioSegmentList* getAudioSegmentList();

    //==============================================================================
    void jobFinished (RenderManager::Job& job, bool completedOk) override;
    void timerCallback() override;

    double clipTimeToSourceFileTime (double clipTime);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipBase)
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::AudioClipBase::FadeBehaviour>
    {
        static tracktion_engine::AudioClipBase::FadeBehaviour fromVar (const var& v)   { return (tracktion_engine::AudioClipBase::FadeBehaviour) static_cast<int> (v); }
        static var toVar (tracktion_engine::AudioClipBase::FadeBehaviour v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion_engine::AudioClipBase::AutoPitchMode>
    {
        static tracktion_engine::AudioClipBase::AutoPitchMode fromVar (const var& v)   { return (tracktion_engine::AudioClipBase::AutoPitchMode) static_cast<int> (v); }
        static var toVar (tracktion_engine::AudioClipBase::AutoPitchMode v)            { return static_cast<int> (v); }
    };
}
