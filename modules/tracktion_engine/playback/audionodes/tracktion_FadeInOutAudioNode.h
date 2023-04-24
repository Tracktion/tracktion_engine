/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

/** An AudioNode that fades its input node in/out at given times. */
class FadeInOutAudioNode : public SingleInputAudioNode
{
public:
    FadeInOutAudioNode (AudioNode* input,
                        legacy::EditTimeRange fadeIn,
                        legacy::EditTimeRange fadeOut,
                        AudioFadeCurve::Type fadeInType,
                        AudioFadeCurve::Type fadeOutType,
                        bool clearSamplesOutsideFade = true);

    ~FadeInOutAudioNode() override;

    static AudioNode* createForEdit (Edit&, AudioNode* input);

    //==============================================================================
    void renderOver (const AudioRenderContext& rc) override;
    void renderAdding (const AudioRenderContext& rc) override;

    void renderSection (const AudioRenderContext&, legacy::EditTimeRange);

private:
    //==============================================================================
    legacy::EditTimeRange fadeIn, fadeOut;
    AudioFadeCurve::Type fadeInType, fadeOutType;
    bool clearExtraSamples = true;

    bool renderingNeeded (const AudioRenderContext&) const;

    static int timeToSample (const AudioRenderContext&, legacy::EditTimeRange, double);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutAudioNode)
};

}} // namespace tracktion { inline namespace engine
