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

//==============================================================================
/**
    ImpulseResponsePlugin that loads an impulse response and applies it the audio stream.
    Additionally this has high and low pass filters to shape the sound.
*/
class ImpulseResponsePlugin  : public Plugin
{
public:
    /** Creates an ImpulseResponsePlugin.
        Initially this will be have no IR loaded, use one of the loadImpulseResponse
        methods to apply it to the audio.
    */
    ImpulseResponsePlugin (PluginCreationInfo);

    /** Destructor. */
    ~ImpulseResponsePlugin() override;

    static const char* getPluginName();
    static inline const char* xmlTypeName = "impulseResponse";

    //==============================================================================
    /** Loads an impulse from binary audio file data i.e. not a block of raw floats.
        @see juce::Convolution::loadImpulseResponse
    */
    bool loadImpulseResponse (const void* sourceData, size_t sourceDataSize);

    /** Loads an impulse from a file.
        @see juce::Convolution::loadImpulseResponse
    */
    bool loadImpulseResponse (const juce::File& fileImpulseResponse);

    /** Loads an impulse from an AudioBuffer<float>.
        @see juce::Convolution::loadImpulseResponse
    */
    bool loadImpulseResponse (juce::AudioBuffer<float>&& bufferImpulseResponse,
                              double sampleRateToStore,
                              int bitDepthToStore);

    //==============================================================================
    juce::CachedValue<juce::String> name;           /**< A name property. This isn't used by the IR itselt but useful in UI contexts. */
    juce::CachedValue<bool> normalise;              /**< Normalise the IR file when loading from the state. True by default. */
    juce::CachedValue<bool> trimSilence;            /**< Trim silence from the IR file when loading from the state. False by default. */

    AutomatableParameter::Ptr highPassCutoffParam;  /**< Cutoff frequency for the high pass filter to applied after the IR */
    AutomatableParameter::Ptr lowPassCutoffParam;   /**< Cutoff frequency for the low pass filter to applied after the IR */
    AutomatableParameter::Ptr gainParam;            /**< Parameter for the gain to apply */
    AutomatableParameter::Ptr mixParam;             /**< Parameter for the mix control, 0.0 = dry, 1.0 = wet */
    AutomatableParameter::Ptr filterQParam;         /**< Parameter for the Q factor of the high pass and low pass filters */

    //==============================================================================
    /** @internal */
    juce::String getName() override;
    /** @internal */
    juce::String getShortName (int /*suggestedLength*/) override;
    /** @internal */
    juce::String getPluginType() override;
    /** @internal */
    bool needsConstantBufferSize() override;
    /** @internal */
    juce::String getSelectableDescription() override;

    /** @internal */
    double getLatencySeconds() override;
    /** @internal */
    void initialise (const PluginInitialisationInfo&) override;
    /** @internal */
    void deinitialise() override;
    /** @internal */
    void reset() override;
    /** @internal */
    void applyToBuffer (const PluginRenderContext&) override;
    /** @internal */
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

private:
    //==============================================================================
    enum
    {
        convolutionIndex,
        HPFIndex,
        LPFIndex,
        gainIndex,
    };

    juce::CachedValue<float> gainValue, mixValue;
    juce::CachedValue<float> highPassCutoffValue, lowPassCutoffValue;
    juce::CachedValue<float> qValue;

    juce::dsp::ProcessorChain<juce::dsp::Convolution,
                              juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>,
                              juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>,
                              juce::dsp::Gain<float>> processorChain;
    juce::SmoothedValue<float> highFreqSmoother, lowFreqSmoother, gainSmoother, wetGainSmoother, dryGainSmoother, qSmoother;

    struct WetDryGain { float wet, dry; };    
    static WetDryGain getWetDryLevels (float mix)
    {
        const float dry = 1.0f - (mix * mix);
        float temp = 1.0f - mix;
        const float wet = 1.0f - (temp * temp);
        
        return { wet, dry };
    }
    void loadImpulseResponseFromState();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpulseResponsePlugin)
};

}} // namespace tracktion { inline namespace engine
