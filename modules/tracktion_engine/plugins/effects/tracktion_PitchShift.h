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

class PitchShiftPlugin : public Plugin
{
public:
    PitchShiftPlugin (Edit&, const juce::ValueTree&);
    PitchShiftPlugin (PluginCreationInfo);
    ~PitchShiftPlugin() override;

    static const char* getPluginName()          { return NEEDS_TRANS("Pitch Shifter"); }
    static juce::ValueTree create();

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() const override       { return TRANS("Pitch Shifter"); }
    juce::String getPluginType() override       { return xmlTypeName; }
    juce::String getShortName (int) override    { return TRANS("Pitch"); }
    bool needsConstantBufferSize() override     { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    double getLatencySeconds() override;
    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> semitonesValue;
    juce::CachedValue<int> mode;
    juce::CachedValue<TimeStretcher::ElastiqueProOptions> elastiqueOptions;
    AutomatableParameter::Ptr semitones;

    static float getMaximumSemitones()   { return 2.0f * 12.0f; }

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftPlugin)
};

}} // namespace tracktion { inline namespace engine
