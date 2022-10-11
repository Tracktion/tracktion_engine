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

/** A test plugin that introduces latency to the incomming signal.
    This isn't added to the PluginManager by default as its main use is for
    internal testing.
*/
class LatencyPlugin  : public Plugin
{
public:
    LatencyPlugin (PluginCreationInfo);
    ~LatencyPlugin() override;

    //==============================================================================
    static const char* getPluginName()                      { return NEEDS_TRANS("Latency Tester"); }
    static const char* xmlTypeName;

    juce::String getName() override                         { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getSelectableDescription() override        { return getName(); }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    double getLatencySeconds() override                     { return latencyTimeSeconds.get(); }
    bool needsConstantBufferSize() override                 { return false; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    //==============================================================================
    ConstrainedCachedValue<float> latencyTimeSeconds;
    juce::CachedValue<bool> applyLatency;

private:
    class DelayRegister;
    std::unique_ptr<DelayRegister> delayCompensator[2];
    LambdaTimer playbackRestartTimer;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatencyPlugin)
};

}} // namespace tracktion { inline namespace engine
