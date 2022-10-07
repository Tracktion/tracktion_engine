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

class ToneGeneratorPlugin   : public Plugin
{
public:
    ToneGeneratorPlugin (PluginCreationInfo);
    ~ToneGeneratorPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Tone Generator"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS(getPluginName()); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return getName(); }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool isSynth() override                             { return true; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmax (1, numInputChannels); }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override    { return getName(); }
    bool needsConstantBufferSize() override             { return false; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    enum class OscType
    {
        sin,
        triangle,
        sawUp,
        sawDown,
        square,
        noise
    };

    static juce::StringArray getOscTypeNames();

    juce::CachedValue<float> oscType, bandLimit, frequency, level;
    AutomatableParameter::Ptr oscTypeParam, bandLimitParam, frequencyParam, levelParam;

private:
    //==============================================================================
    juce::AudioBuffer<float> scratch;
    std::atomic<bool> bandLimitOsc { false };

    juce::dsp::Oscillator<float> sine;
    juce::dsp::Oscillator<float> triangle;
    juce::dsp::Oscillator<float> sawUp;
    juce::dsp::Oscillator<float> sawDown;
    juce::dsp::Oscillator<float> square;
    juce::dsp::Oscillator<float> noise;

    void initialiseOscilators();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneGeneratorPlugin)
};

}} // namespace tracktion { inline namespace engine
