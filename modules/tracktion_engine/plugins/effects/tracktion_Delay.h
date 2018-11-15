/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct DelayBufferBase
{
    DelayBufferBase()  {}

    void ensureMaxBufferSize (int size)
    {
        if (++size > bufferSamples)
        {
            bufferSamples = size;
            buffers[0].ensureSize ((size_t) bufferSamples * sizeof (float) + 32, true);
            buffers[1].ensureSize ((size_t) bufferSamples * sizeof (float) + 32, true);

            if (bufferPos >= bufferSamples)
                bufferPos = 0;
        }
    }

    void clearBuffer()
    {
        buffers[0].fillWith (0);
        buffers[1].fillWith (0);
    }

    void releaseBuffer()
    {
        bufferSamples = 0;
        bufferPos = 0;
        buffers[0].setSize (0);
        buffers[1].setSize (0);
    }

    int bufferPos = 0, bufferSamples = 0;
    juce::MemoryBlock buffers[2];

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayBufferBase)
};


//==============================================================================
class DelayPlugin  : public Plugin
{
public:
    DelayPlugin (PluginCreationInfo);
    ~DelayPlugin();

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Delay"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("Delay"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getSelectableDescription() override    { return TRANS("Delay Plugin"); }
    bool needsConstantBufferSize() override             { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return juce::jmin (numInputChannels, 2); }
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void reset() override;
    void applyToBuffer (const AudioRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> feedbackValue, mixValue;
    juce::CachedValue<int> lengthMs;

    AutomatableParameter::Ptr feedbackDb, mixProportion;

    static float getMinDelayFeedbackDb() noexcept       { return -30.0f; }


private:
    DelayBufferBase delayBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayPlugin)
};

} // namespace tracktion_engine
