/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

using namespace juce;

namespace tracktion_engine
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
    ImpulseResponsePlugin (PluginCreationInfo info);

    /** Destructor. */
    ~ImpulseResponsePlugin() override;

    static const char* getPluginName();
    static inline const char* xmlTypeName = "impulseResponse";

    //==============================================================================
    /** Loads an impulse from binary audio file data i.e. not a block of raw floats.
        @see juce::Convolution::loadImpulseResponse
    */
    void loadImpulseResponse (const void* sourceData, size_t sourceDataSize,
        dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, size_t size,
        dsp::Convolution::Normalise requiresNormalisation);

    /** Loads an impulse from a file.
        @see juce::Convolution::loadImpulseResponse
    */
    void loadImpulseResponse (const File& fileImpulseResponse, size_t sizeInBytes, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming);

    /** Loads an impulse from an AudioBuffer<float>.
        @see juce::Convolution::loadImpulseResponse
    */
    void loadImpulseResponse (AudioBuffer<float>&& bufferImpulseResponse, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, dsp::Convolution::Normalise requiresNormalisation);

    //==============================================================================
    juce::CachedValue<float> preGainValue, postGainValue;
    juce::CachedValue<float> highPassCutoffValue, lowPassCutoffValue;

    AutomatableParameter::Ptr preGainParam;         /**< Parameter for the Gain to apply before the IR */
    AutomatableParameter::Ptr highPassCutoffParam;  /**< Cutoff frequency for the high pass filter to applied after the IR */
    AutomatableParameter::Ptr lowPassCutoffParam;   /**< Cutoff frequency for the low pass filter to applied after the IR */
    AutomatableParameter::Ptr postGainParam;        /**< Parameter for the Gain to apply after the IR */

    //==============================================================================
    /** @internal */
    juce::String getName() override;
    /** @internal */
    juce::String getPluginType() override;
    /** @internal */
    bool needsConstantBufferSize() override;
    /** @internal */
    juce::String getSelectableDescription() override;

    /** @internal */
    double getLatencySeconds() override;

    /** @internal */
    void initialise (const PluginInitialisationInfo& info) override;

    /** @internal */
    void deinitialise() override;

    /** @internal */
    void reset() override;

    /** @internal */
    void applyToBuffer (const PluginRenderContext& fc) override;

    /** @internal */
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override;

private:
    //==============================================================================
    enum
    {
        preGainIndex,
        convolutionIndex,
        HPFIndex,
        LPFIndex,
        postGainIndex
    };

    dsp::ProcessorChain<dsp::Gain<float>,
                        dsp::Convolution,
                        dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>,
                        dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>,
                        dsp::Gain<float>> processorChain;


    void loadImpulseResponseFromState();


    void valueTreePropertyChanged(ValueTree& v, const juce::Identifier& id) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpulseResponsePlugin)
};

}