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
class ClipEffect    : protected ValueTreeAllEventListener
{
public:
    enum class EffectType
    {
        none,
        volume,
        fadeInOut,
        tapeStartStop,
        stepVolume,
        pitchShift,
        warpTime,
        normalise,
        makeMono,
        reverse,
        invert,
        filter,
        firstEffect     = volume,
        lastEffect      = filter
    };

    static juce::ValueTree create (EffectType);
    static ClipEffect* create (const juce::ValueTree&, ClipEffects&);

    static void createEffectAndAddToValueTree (Edit&, juce::ValueTree parent,
                                               ClipEffect::EffectType, int index);

    static juce::String getTypeDisplayName (EffectType);
    static void addEffectsToMenu (juce::PopupMenu&);

    ClipEffect (const juce::ValueTree&, ClipEffects&);

    EffectType getType() const;

    struct ClipEffectRenderJob;

    /** Subclasses should return a job that can render the source.
        N.B. because the sourceFile may not be valid at the time of job creation you should use
        the sourceLength parameter to determine how to build the render node.
    */
    virtual juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) = 0;

    /** Return true here to show a properties button in the editor and enable the propertiesButtonPressed callback. */
    virtual bool hasProperties()                                { return false; }
    virtual void propertiesButtonPressed (SelectionManager&)    {}

    /** Callback to indicate the destination file has changed. */
    virtual void sourceChanged() {}

    /** Returns the hash for this effect.
        N.B. as effects are serial their hash will change if any preceeding effects change.
       @see getIndividualHash
    */
    juce::int64 getHash() const;

    AudioFile getSourceFile() const;
    AudioFile getDestinationFile() const;

    bool isUsingFile (const AudioFile&) const;

    virtual void flushStateToValueTree() {};

    juce::UndoManager& getUndoManager();
    AudioClipBase& getClip();

    Edit& edit;
    juce::ValueTree state;
    ClipEffects& clipEffects;

protected:
    virtual juce::int64 getIndividualHash() const;
    void valueTreeChanged() override;

private:
    friend class ClipEffects;
    mutable AudioFile destinationFile;

    void invalidateDestination();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipEffect)
};

//==============================================================================
/**
*/
class ClipEffects   : public ValueTreeObjectList<ClipEffect>,
                      private juce::Timer
{
public:
    static juce::ValueTree create()
    {
        return juce::ValueTree (IDs::EFFECTS);
    }

    //==============================================================================
    ClipEffects (const juce::ValueTree&, AudioClipBase&);
    ~ClipEffects();

    void flushStateToValueTree()
    {
        for (auto ce : objects)
            ce->flushStateToValueTree();
    }

    ClipEffect* getClipEffect (const juce::ValueTree& v)
    {
        for (auto ce : objects)
            if (ce->state == v)
                return ce;

        return {};
    }

    /** Returns the hash for this set of effects. */
    juce::int64 getHash() const
    {
        if (cachedHash == hashNeedsRecaching)
            if (auto ce = objects.getLast())
                cachedHash = ce->getHash();

        return cachedHash;
    }

    /** Returns the start position in the file that the effect should apply to.
        In practice this is the loop start point.
    */
    double getEffectsStartTime() const
    {
        return getCachedClipProperties().effectsRange.getStart();
    }

    /** Returns the length of the effect. This will be the effective length of the source file
        i.e. taking into account loop length and speed ratio.
    */
    double getEffectsLength() const
    {
        return getCachedClipProperties().effectsRange.getLength();
    }

    /** Returns the range of the file that the effect should apply to.
        In practice this is the loop start point and loop length.
    */
    EditTimeRange getEffectsRange() const
    {
        return getCachedClipProperties().effectsRange;
    }

    /** Returns the speed ratio of the clip or an estimate of this if the clip is auto tempo.
        Note that because Edits can have multiple tempo changes this will only be an estimate based
        on the tempo at the start of the clip.
    */
    double getSpeedRatioEstimate() const
    {
        return getCachedClipProperties().speedRatio;
    }

    bool isUsingFile (const AudioFile& af)
    {
        for (auto ce : objects)
            if (ce->isUsingFile (af))
                return true;

        return false;
    }

    void notifyListenersOfRenderCompletion()
    {
        listeners.call (&Listener::renderComplete);
    }

    RenderManager::Job::Ptr createRenderJob (const AudioFile& destFile, const AudioFile& sourceFile) const;

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::EFFECT);
    }

    ClipEffect* createNewObject (const juce::ValueTree& v) override
    {
        auto ce = ClipEffect::create (v, *this);
        jassert (ce != nullptr);
        listeners.call (&Listener::effectAdded, ce);
        return ce;
    }

    void deleteObject (ClipEffect* ce) override
    {
        listeners.call (&Listener::effectRemoved, ce);
        jassert (ce != nullptr);
        delete ce;
    }

    void newObjectAdded (ClipEffect*) override
    {
        invalidateAllEffects();
    }

    void objectRemoved (ClipEffect*) override
    {
        invalidateAllEffects();
    }

    void objectOrderChanged() override
    {
        invalidateAllEffects();
        effectChanged();
        listeners.call (&Listener::effectsReordered);
    }

    struct Listener
    {
        virtual ~Listener() {}
        virtual void effectChanged() {}
        virtual void effectAdded (ClipEffect*) {}
        virtual void effectRemoved (ClipEffect*) {}
        virtual void effectsReordered() {}
        virtual void renderComplete() {}
    };

    void addListener (Listener* l)          { listeners.add (l); };
    void removeListener (Listener* l)       { listeners.remove (l); };

    struct RenderInhibitor
    {
        RenderInhibitor (ClipEffects& o) : effects (o)  { ++effects.renderInhibitors; }
        ~RenderInhibitor()                              { --effects.renderInhibitors; }

    private:
        ClipEffects& effects;
        JUCE_DECLARE_NON_COPYABLE (RenderInhibitor)
    };

    AudioClipBase& clip;
    juce::ValueTree state;

private:
    enum { hashNeedsRecaching = -1 };

    struct CachedClipProperties
    {
        CachedClipProperties (const ClipEffects& ce)
        {
            auto& c = ce.clip;
            speedRatio = c.getSpeedRatio();

            if (c.getAutoTempo())
            {
                // need to find the tempo changes between the source start and  how long the result will be
                auto info = AudioFile (c.getOriginalFile()).getInfo();
                auto sourceTempo = c.getLoopInfo().getBpm (info);
                speedRatio = c.edit.tempoSequence.getTempoAt (c.getPosition().time.start).getBpm() / sourceTempo;
            }

            if (c.isLooping())
                effectsRange = { c.getLoopStart(), c.getLoopStart() + c.getLoopLength() };
            else
                effectsRange = { 0.0, c.getSourceLength() / speedRatio };
        }

        EditTimeRange effectsRange;
        double speedRatio = 1.0;
    };

    struct ClipPropertyWatcher;
    friend class ClipEffect;

    juce::ListenerList<Listener> listeners;
    std::unique_ptr<ClipPropertyWatcher> clipPropertyWatcher;
    mutable std::unique_ptr<CachedClipProperties> cachedClipProperties;
    mutable juce::int64 cachedHash = hashNeedsRecaching;

    int renderInhibitors = 0;

    const CachedClipProperties& getCachedClipProperties() const
    {
        if (cachedClipProperties == nullptr)
            cachedClipProperties.reset (new CachedClipProperties (*this));

        return *cachedClipProperties;
    }

    void invalidateAllEffects()
    {
        cachedHash = hashNeedsRecaching;

        for (auto ce : objects)
            ce->invalidateDestination();
    }

    void effectChanged()
    {
        clip.cancelCurrentRender();
        startTimer (500);
    }

    void timerCallback() override
    {
        if (renderInhibitors > 0)
            return;

        invalidateAllEffects();
        listeners.call (&Listener::effectChanged);
        stopTimer();
    }

    JUCE_DECLARE_WEAK_REFERENCEABLE (ClipEffects)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipEffects)
};

//==============================================================================
/**
*/
struct ScopedPluginUnloadInhibitor
{
    ScopedPluginUnloadInhibitor (PluginUnloadInhibitor&);
    ~ScopedPluginUnloadInhibitor();

    PluginUnloadInhibitor& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedPluginUnloadInhibitor)
};

//==============================================================================
/**
*/
struct VolumeEffect : public ClipEffect,
                      private juce::Timer
{
    VolumeEffect (const juce::ValueTree&, ClipEffects&);
    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) override;

    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;
    juce::int64 getIndividualHash() const override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChanged() override;
    void timerCallback() override;

    VolumeAndPanPlugin::Ptr plugin;
    std::unique_ptr<ClipEffects::RenderInhibitor> inhibitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeEffect)
};

//==============================================================================
/**
*/
struct FadeInOutEffect  : public ClipEffect
{
    FadeInOutEffect (const juce::ValueTree&, ClipEffects&);

    void setFadeIn (double);
    void setFadeOut (double);

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) override;

    juce::CachedValue<double> fadeIn, fadeOut;
    juce::CachedValue<AudioFadeCurve::Type> fadeInType, fadeOutType;

protected:
    juce::int64 getIndividualHash() const override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutEffect)
};

//==============================================================================
/**
*/
struct StepVolumeEffect  : public ClipEffect,
                           public Selectable
{
    StepVolumeEffect (const juce::ValueTree&, ClipEffects&);
    ~StepVolumeEffect();

    int getMaxNumNotes();

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) override;

    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;

    juce::String getSelectableDescription() override;

    //==============================================================================
    struct Pattern
    {
        Pattern (StepVolumeEffect&);
        Pattern (const Pattern& other) noexcept;

        bool getNote (int index) const noexcept;
        void setNote (int index, bool value);

        juce::BigInteger getPattern() const noexcept;
        void setPattern (const juce::BigInteger&) noexcept;

        void clear();
        int getNumNotes() const;

        void shiftChannel (bool toTheRight);
        void toggleAtInterval (int interval);
        void randomiseChannel();

        StepVolumeEffect& effect;
        juce::ValueTree state;

    private:
        Pattern& operator= (const Pattern&) = delete;
        JUCE_LEAK_DETECTOR (Pattern)
    };

    juce::CachedValue<double> noteLength, crossfade;
    juce::CachedValue<juce::String> pattern;

protected:
    juce::int64 getIndividualHash() const override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepVolumeEffect)
};

//==============================================================================
/**
*/
struct PitchShiftEffect  : public ClipEffect,
                           private juce::Timer
{
    PitchShiftEffect (const juce::ValueTree&, ClipEffects&);

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) override;

    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;
    juce::int64 getIndividualHash() const override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChanged() override;
    void timerCallback() override;

    juce::ReferenceCountedObjectPtr<PitchShiftPlugin> plugin;
    std::unique_ptr<ClipEffects::RenderInhibitor> inhibitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftEffect)
};

//==============================================================================
/**
*/
struct WarpTimeEffect   : public ClipEffect
{
    WarpTimeEffect (const juce::ValueTree&, ClipEffects&);

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile&, double sourceLength) override;

    juce::int64 getIndividualHash() const override;
    void sourceChanged() override;

    WarpTimeManager::Ptr warpTimeManager;
    std::unique_ptr<Edit::LoadFinishedCallback<WarpTimeEffect>> editLoadedCallback;

    void editFinishedLoading();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeEffect)
};

//==============================================================================
/**
*/
struct PluginEffect  : public ClipEffect,
                       public AutomatableParameter::Listener,
                       private juce::Timer
{
    PluginEffect (const juce::ValueTree&, ClipEffects&);

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile&, double sourceLength) override;

    void flushStateToValueTree() override;
    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;
    juce::int64 getIndividualHash() const override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChanged() override;

    void timerCallback() override;
    void curveHasChanged (AutomatableParameter&) override;

    Plugin::Ptr plugin;
    std::unique_ptr<ClipEffects::RenderInhibitor> inhibitor;
    std::unique_ptr<PluginUnloadInhibitor> pluginUnloadInhibitor;
    juce::CachedValue<int> currentCurve;
    mutable juce::CachedValue<juce::int64> lastHash;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEffect)
};

//==============================================================================
/**
*/
struct NormaliseEffect  : public ClipEffect,
                          public Selectable
{
    NormaliseEffect (const juce::ValueTree&, ClipEffects&);
    ~NormaliseEffect();

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile& sourceFile, double sourceLength) override;

    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;
    juce::String getSelectableDescription() override;

    juce::CachedValue<double> maxLevelDB;

private:
    struct NormaliseRenderJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NormaliseEffect)
};

//==============================================================================
/**
*/
struct MakeMonoEffect : public ClipEffect,
                        public Selectable
{
    enum SrcChannels
    {
        chLR = 0,
        chL  = 1,
        chR  = 2
    };

    MakeMonoEffect (const juce::ValueTree&, ClipEffects&);
    ~MakeMonoEffect();

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile&, double sourceLength) override;

    bool hasProperties() override;
    void propertiesButtonPressed (SelectionManager&) override;
    juce::String getSelectableDescription() override;

    juce::CachedValue<int> channels;

private:
    struct MakeMonoRenderJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MakeMonoEffect)
};

//==============================================================================
/**
*/
struct ReverseEffect  : public ClipEffect
{
    ReverseEffect (const juce::ValueTree&, ClipEffects&);

    juce::ReferenceCountedObjectPtr<ClipEffectRenderJob> createRenderJob (const AudioFile&, double sourceLength) override;

    struct ReverseRenderJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseEffect)
};

//==============================================================================
/**
*/
struct InvertEffect  : public ClipEffect
{
    InvertEffect (const juce::ValueTree&, ClipEffects&);

    juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> createRenderJob (const AudioFile&, double sourceLength) override;

    struct InvertRenderJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InvertEffect)
};

} // namespace tracktion_engine
