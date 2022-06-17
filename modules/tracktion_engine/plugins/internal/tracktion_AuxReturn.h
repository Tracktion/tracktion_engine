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

class AuxReturnPlugin   : public Plugin
{
public:
    AuxReturnPlugin (PluginCreationInfo);
    ~AuxReturnPlugin() override;

    static const char* getPluginName()                                      { return NEEDS_TRANS("Aux Return"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getPluginType() override                                   { return xmlTypeName; }
    juce::String getShortName (int suggestedMaxLength) override;
    juce::String getSelectableDescription() override                        { return TRANS("Aux Return Plugin"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return juce::jmin (numInputChannels, 2); }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    bool takesAudioInput() override                  { return true; }
    bool takesMidiInput() override                   { return true; }
    bool producesAudioWhenNoAudioInput() override    { return true; }
    bool canBeAddedToClip() override                 { return false; }
    bool canBeAddedToRack() override                 { return false; }
    bool needsConstantBufferSize() override          { return true; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<int> busNumber;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxReturnPlugin)
};

}} // namespace tracktion { inline namespace engine
