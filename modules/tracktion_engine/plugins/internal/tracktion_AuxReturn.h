/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AuxReturnPlugin   : public Plugin
{
public:
    AuxReturnPlugin (PluginCreationInfo);
    ~AuxReturnPlugin();

    static const char* getPluginName()                                      { return NEEDS_TRANS("Aux Return"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getPluginType() override                                   { return xmlTypeName; }
    juce::String getShortName (int suggestedMaxLength) override;
    juce::String getSelectableDescription() override                        { return TRANS("Aux Return Plugin"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return juce::jmin (numInputChannels, 2); }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void prepareForNextBlock (const AudioRenderContext&) override;
    void applyToBuffer (const AudioRenderContext&) override;

    bool takesAudioInput() override                  { return true; }
    bool takesMidiInput() override                   { return true; }
    bool producesAudioWhenNoAudioInput() override    { return true; }
    bool canBeAddedToClip() override                 { return false; }
    bool canBeAddedToRack() override                 { return false; }
    bool needsConstantBufferSize() override          { return true; }

    void applyAudioFromSend (const juce::AudioBuffer<float>&, int startSample, int numSamples, float gain1, float gain2);
    void applyAudioFromSend (const juce::AudioBuffer<float>&, int startSample, int numSamples, float gain);

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<int> busNumber;

private:
    std::unique_ptr<juce::AudioBuffer<float>> previousBuffer, currentBuffer;
    int samplesInPreviousBuffer, samplesInCurrentBuffer;
    bool initialised = false;
    juce::CriticalSection addingAudioLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxReturnPlugin)
};

} // namespace tracktion_engine
