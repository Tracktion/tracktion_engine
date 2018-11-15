/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that fades its input node in/out at given times. */
class FadeInOutAudioNode : public SingleInputAudioNode
{
public:
    FadeInOutAudioNode (AudioNode* input,
                        EditTimeRange fadeIn,
                        EditTimeRange fadeOut,
                        AudioFadeCurve::Type fadeInType,
                        AudioFadeCurve::Type fadeOutType,
                        bool clearSamplesOutsideFade = true);

    ~FadeInOutAudioNode();

    static AudioNode* createForEdit (Edit&, AudioNode* input);

    //==============================================================================
    void renderOver (const AudioRenderContext& rc) override;
    void renderAdding (const AudioRenderContext& rc) override;

    void renderSection (const AudioRenderContext&, EditTimeRange editTime);

private:
    //==============================================================================
    EditTimeRange fadeIn, fadeOut;
    AudioFadeCurve::Type fadeInType, fadeOutType;
    bool clearExtraSamples = true;

    bool renderingNeeded (const AudioRenderContext&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutAudioNode)
};

} // namespace tracktion_engine
