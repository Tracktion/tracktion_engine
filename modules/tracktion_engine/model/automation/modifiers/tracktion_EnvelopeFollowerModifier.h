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

/** */
class EnvelopeFollowerModifier  : public Modifier,
                                  private ValueTreeAllEventListener
{
public:
    //==============================================================================
    EnvelopeFollowerModifier (Edit&, const juce::ValueTree&);
    ~EnvelopeFollowerModifier() override;

    using Ptr    = juce::ReferenceCountedObjectPtr<EnvelopeFollowerModifier>;
    using Array = juce::ReferenceCountedArray<EnvelopeFollowerModifier>;

    void initialise() override  {}
    float getCurrentValue() override;

    juce::String getName() const override           { return TRANS("Envelope Follower Modifier"); }

    //==============================================================================
    float getEnvelopeValue() const noexcept         { return envelopeValue.load (std::memory_order_acquire); }

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    juce::StringArray getAudioInputNames() override;
    ProcessingPosition getProcessingPosition() override { return ProcessingPosition::postFX; }

    void initialise (double sampleRate, int blockSizeSamples) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    //==============================================================================
    struct Assignment : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const EnvelopeFollowerModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
        EnvelopeFollowerModifier::Ptr getEnvelopeFollowerModifier() const;

        const EditItemID envelopeFollowerModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override    { return getName(); }

    //==============================================================================
    juce::CachedValue<float> gainDb, attack, hold, release, depth, offset, lowPassEnabled, highPassEnabled, lowPassFrequency, highPassFrequency;
    AutomatableParameter::Ptr gainDbParam, attackParam, holdParam, releaseParam, depthParam, offsetParam,
        lowPassEnabledParam, highPassEnabledParam, lowPassFrequencyParam, highPassFrequencyParam;

private:
    class EnvelopeFollower;

    std::atomic<float> envelopeValue;
    std::unique_ptr<EnvelopeFollower> envelopeFollower;
    juce::IIRFilter lowPassFilter, highPassFilter;
    float currentLowPassFrequency = 0.0f, currentHighPassFrequency = 0.0f;
    LambdaTimer changedTimer;

    void prepareToPlay (double sampleRate);
    void processBlock (const juce::AudioBuffer<float>&);
    void reset();

    void valueTreeChanged() override;
};

}} // namespace tracktion { inline namespace engine
