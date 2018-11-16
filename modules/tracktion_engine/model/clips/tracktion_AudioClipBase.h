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
    ~AudioClipBase();

    //==============================================================================
    void initialise() override;
    void cloneFrom (Clip*) override;
    void flushStateToValueTree() override;

    //==============================================================================
    void setTrack (ClipTrack*) override;
    bool canGoOnTrack (Track&) override;
    void changed() override;

    //==============================================================================
    juce::Colour getDefaultColour() const override;

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
    virtual AudioFile getAudioFile() const  { return AudioFile (getCurrentSourceFile()); }

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

    void cancelCurrentRender();

    //==============================================================================
    void setGainDB (float dB);
    float getGainDB() const noexcept                    { return dbGain; }
    float getGain() const noexcept                      { return gain; }

    void setPan (float pan);
    float getPan() const noexcept                       { return pan; }

    void setMuted (bool shouldBeMuted) override         { mute = shouldBeMuted; }
    bool isMuted() const override                       { return mute; }

    LiveClipLevel getLiveClipLevel();

    //==============================================================================
    void setLeftChannelActive (bool);
    bool isLeftChannelActive() const;

    void setRightChannelActive (bool);
    bool isRightChannelActive() const;

    //==============================================================================
    bool setFadeIn (double length);
    double getFadeIn() const;

    bool setFadeOut (double length);
    double getFadeOut() const;

    void setFadeInType (AudioFadeCurve::Type newType);
    AudioFadeCurve::Type getFadeInType() const          { return fadeInType; }

    void setFadeOutType (AudioFadeCurve::Type newType);
    AudioFadeCurve::Type getFadeOutType() const         { return fadeOutType; }

    void setAutoCrossfade (bool shouldAutoCrossfade)    { autoCrossfade = shouldAutoCrossfade; }
    bool getAutoCrossfade() const noexcept              { return autoCrossfade; }

    void updateAutoCrossfadesAsync (bool updateOverlapped);
    void applyEdgeFades();
    void copyFadeToAutomation (bool fadeIn, bool removeClipFade);

    //==============================================================================
    enum FadeBehaviour
    {
        gainFade = 0,
        speedRamp
    };

    void setFadeInBehaviour (FadeBehaviour newBehaviour)    { fadeInBehaviour = newBehaviour; }
    FadeBehaviour getFadeInBehaviour() const                { return fadeInBehaviour; }

    void setFadeOutBehaviour (FadeBehaviour newBehaviour)   { fadeOutBehaviour = newBehaviour; }
    FadeBehaviour getFadeOutBehaviour() const               { return fadeOutBehaviour; }

    //==============================================================================
    /** Override this to fill in the LoopInfo structure as best fits the source. */
    virtual void setLoopDefaults() = 0;

    void setLoopInfo (const LoopInfo&);
    const LoopInfo& getLoopInfo() const                 { return loopInfo; }
    LoopInfo& getLoopInfo()                             { return loopInfo; }

    bool canLoop() const override                       { return ! isUsingMelodyne(); }
    bool isLooping() const override                     { return getAutoTempo() ? (loopLengthBeats > 0.0) : (loopLength > 0.0); }
    bool beatBasedLooping() const override              { return isLooping() && getAutoTempo(); }
    void setNumberOfLoops (int num) override;
    void disableLooping() override;

    EditTimeRange getLoopRange() const;
    double getLoopStartBeats() const override;
    double getLoopStart() const override;
    double getLoopLengthBeats() const override;
    double getLoopLength() const override;

    void setLoopRange (EditTimeRange) override;
    void setLoopRangeBeats (juce::Range<double>) override;

    void setAutoDetectBeats (bool);
    bool getAutoDetectBeats() const                     { return autoDetectBeats; }

    void setBeatSensitivity (float s);
    float getBeatSensitivity() const                    { return beatSensitivity; }

    void pitchTempoTrackChanged() override;
    void clearCachedAudioSegmentList();
    const AudioSegmentList* getAudioSegmentList();

    //==============================================================================
    void setSpeedRatio (double newSpeed) override;
    void setTimeStretchMode (TimeStretcher::Mode mode);
    TimeStretcher::Mode getTimeStretchMode() const noexcept;
    TimeStretcher::Mode getActualTimeStretchMode() const noexcept;

    void setAutoTempo (bool shouldUseAutoTempo)         { autoTempo = shouldUseAutoTempo; }
    bool getAutoTempo() const                           { return autoTempo; }
    void setAutoPitch (bool shouldUseAutoPitch)         { autoPitch = shouldUseAutoPitch; }
    bool getAutoPitch() const                           { return autoPitch; }
    void setWarpTime (bool shouldUseWarpTime)           { warpTime = shouldUseWarpTime; }
    bool getWarpTime() const                            { return warpTime; }
    WarpTimeManager& getWarpTimeManager() const;

    void setTranspose (int numSemitones)                { transpose = juce::jlimit (-24, 24, numSemitones); }
    int getTransposeSemiTones (bool includeAutoPitch) const;

    void setPitchChange (float semitones)               { pitchChange = juce::jlimit (-48.0f, 48.0f, semitones); }
    float getPitchChange() const                        { return pitchChange; }
    float getPitchRatio() const                         { return juce::jlimit (0.1f, 10.0f, std::pow (2.0f, pitchChange / 12.0f)); }

    void setIsReversed (bool shouldBeReversed)          { isReversed = shouldBeReversed; }
    bool getIsReversed() const noexcept                 { return isReversed; }

    LoopInfo autoDetectBeatMarkers (const LoopInfo& current, bool autoBeat, float sens) const;
    bool performTempoDetect();

    static juce::StringArray getRootNoteChoices (Engine&);
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
    /** Creates the AudioNode used to playback this Clip in an Edit. */
    AudioNode* createAudioNode (const CreateAudioNodeParams&) override;
    AudioNode* createMelodyneAudioNode();

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

    static juce::String getClipProxyPrefix()    { return "clip_"; }
    static juce::String getFileProxyPrefix()    { return "proxy_"; }

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
    void loadMelodyneState (Edit&);
    void showMelodyneWindow();
    void melodyneConvertToMIDI();

    juce::CachedValue<TimeStretcher::ElastiqueProOptions> elastiqueProOptions;

protected:
    //==============================================================================
    friend class MelodyneAudioNode;
    friend class WaveCompManager;

    //==============================================================================
    float gain = 1.0f;
    juce::CachedValue<float> dbGain, pan;
    juce::CachedValue<bool> mute;
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
    class TimestretchingPreviewAudioNode;
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

    //==============================================================================
    void jobFinished (RenderManager::Job& job, bool completedOk) override;
    void timerCallback() override;

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
    bool setupARA (Edit&, bool dontPopupErrorMessages);

    AudioNode* createNode (EditTimeRange editTime, LiveClipLevel, bool includeMelodyne);

    AudioNode* createFadeInOutNode (AudioNode*);

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
}
