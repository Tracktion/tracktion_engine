/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class FreezePointPlugin    : public Plugin
{
public:
    // matches the global settings
    enum Position
    {
        beforeAllPlugins = 0,
        preFader,
        postFader
    };

    /** Temporarily unsolos all the tracks in an Edit. */
    struct ScopedTrackUnsoloer
    {
        ScopedTrackUnsoloer (Edit& e);
        ~ScopedTrackUnsoloer();

        Edit& edit;
        juce::BigInteger soloed, soloIsolated;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedTrackUnsoloer)
    };

    /** Temporarily solo isolates and unmutes some tracks. Useful for rendering */
    struct ScopedTrackSoloIsolator
    {
        ScopedTrackSoloIsolator (Edit& e, Track::Array&);
        ~ScopedTrackSoloIsolator();

        Edit& edit;
        Track::Array tracks;
        juce::BigInteger muted, notSoloIsolated;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedTrackSoloIsolator)
    };

    /** Temporarily disables a range of plugins in a track. */
    struct ScopedPluginDisabler
    {
        ScopedPluginDisabler (Track& track, juce::Range<int> pluginsToDisable);
        ~ScopedPluginDisabler();

    private:
        Track& track;
        juce::Range<int> indexes;
        juce::BigInteger enabled;

        void enablePlugins (bool enable);
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedPluginDisabler)
    };

    /** Freezes a track if this plugin changes track. */
    struct ScopedTrackFreezer
    {
        ScopedTrackFreezer (FreezePointPlugin& o);
        ~ScopedTrackFreezer();

        EditItemID trackID;
        FreezePointPlugin& owner;
        bool wasFrozen;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedTrackFreezer)
    };

    static ScopedTrackFreezer* createTrackFreezer (const Plugin::Ptr&);

    FreezePointPlugin (PluginCreationInfo);
    ~FreezePointPlugin();

    static const char* getPluginName()      { return NEEDS_TRANS("Freeze Point"); }
    static juce::ValueTree create();

    //==============================================================================
    static const char* xmlTypeName;

    void initialiseFully() override;
    juce::String getName() override                     { return TRANS("Freeze"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getTooltip() override                  { return TRANS("Track will freeze up to this plugin"); }
    juce::String getSelectableDescription() override    { return TRANS("Freeze Point Plugin"); }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    bool canBeAddedToClip() override                    { return false; }
    bool canBeAddedToRack() override                    { return false; }
    bool canBeAddedToMaster() override                  { return false; }
    bool canBeDisabled() override                       { return false; }
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;
    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return numInputChannels; }

    bool isTrackFrozen() const;
    void freezeTrack (bool shouldBeFrozen);

private:
    //==============================================================================
    int lastFreezeIndex = -1;

    void updateTrackFreezeStatus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezePointPlugin)
};

} // namespace tracktion_engine
