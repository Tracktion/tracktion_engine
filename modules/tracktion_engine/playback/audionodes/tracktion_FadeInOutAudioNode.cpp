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

FadeInOutAudioNode::FadeInOutAudioNode (AudioNode* inp,
                                        legacy::EditTimeRange in, legacy::EditTimeRange out,
                                        AudioFadeCurve::Type fadeInType_,
                                        AudioFadeCurve::Type fadeOutType_,
                                        bool clearSamplesOutsideFade)
    : SingleInputAudioNode (inp),
      fadeIn (in),
      fadeOut (out),
      fadeInType (fadeInType_),
      fadeOutType (fadeOutType_),
      clearExtraSamples (clearSamplesOutsideFade)
{
    jassert (! (fadeIn.isEmpty() && fadeOut.isEmpty()));
}

FadeInOutAudioNode::~FadeInOutAudioNode()
{
}

int FadeInOutAudioNode::timeToSample (const AudioRenderContext& rc, legacy::EditTimeRange editTime, double t)
{
    return (int) (rc.bufferNumSamples * (t - editTime.getStart()) / editTime.getLength() + 0.5);
}

void FadeInOutAudioNode::renderSection (const AudioRenderContext& rc, legacy::EditTimeRange editTime)
{
    if (editTime.overlaps (fadeIn) && fadeIn.getLength() > 0.0)
    {
        double alpha1 = 0;
        auto startSamp = timeToSample (rc, editTime, fadeIn.getStart());

        if (startSamp > 0)
        {
            if (clearExtraSamples)
                rc.destBuffer->clear (rc.bufferStartSample, startSamp);
        }
        else
        {
            alpha1 = (editTime.getStart() - fadeIn.getStart()) / fadeIn.getLength();
            startSamp = 0;
        }

        int endSamp;
        double alpha2;

        if (editTime.getEnd() >= fadeIn.getEnd())
        {
            endSamp = timeToSample (rc, editTime, fadeIn.getEnd());
            alpha2 = 1.0;
        }
        else
        {
            endSamp = rc.bufferNumSamples;
            alpha2 = std::max (0.0, (editTime.getEnd() - fadeIn.getStart()) / fadeIn.getLength());
        }

        if (endSamp > startSamp)
            AudioFadeCurve::applyCrossfadeSection (*rc.destBuffer,
                                                   rc.bufferStartSample + startSamp, endSamp - startSamp,
                                                   fadeInType,
                                                   (float) alpha1,
                                                   (float) alpha2);
    }

    if (editTime.overlaps (fadeOut) && fadeOut.getLength() > 0.0)
    {
        double alpha1 = 0;
        auto startSamp = timeToSample (rc, editTime, fadeOut.getStart());

        if (startSamp <= 0)
        {
            startSamp = 0;
            alpha1 = (editTime.getStart() - fadeOut.getStart()) / fadeOut.getLength();
        }

        int endSamp;
        double alpha2;

        if (editTime.getEnd() >= fadeOut.getEnd())
        {
            endSamp = timeToSample (rc, editTime, fadeOut.getEnd());
            alpha2 = 1.0;

            if (clearExtraSamples && endSamp < rc.bufferNumSamples)
                rc.destBuffer->clear (rc.bufferStartSample + endSamp,
                                      rc.bufferNumSamples - endSamp);
        }
        else
        {
            endSamp = rc.bufferNumSamples;
            alpha2 = (editTime.getEnd() - fadeOut.getStart()) / fadeOut.getLength();
        }

        if (endSamp > startSamp)
            AudioFadeCurve::applyCrossfadeSection (*rc.destBuffer,
                                                   rc.bufferStartSample + startSamp, endSamp - startSamp,
                                                   fadeOutType,
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha1)),
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha2)));
    }
}

bool FadeInOutAudioNode::renderingNeeded (const AudioRenderContext& rc) const
{
    if (rc.destBuffer == nullptr || ! rc.playhead.isPlaying())
        return false;

    auto editTime = rc.getEditTime();

    if (editTime.isSplit)
    {
        return fadeIn.overlaps (editTime.editRange1)
            || fadeIn.overlaps (editTime.editRange2)
            || fadeOut.overlaps (editTime.editRange1)
            || fadeOut.overlaps (editTime.editRange2);
    }

    return fadeIn.overlaps (editTime.editRange1)
        || fadeOut.overlaps (editTime.editRange1);
}

void FadeInOutAudioNode::renderOver (const AudioRenderContext& rc)
{
    input->renderOver (rc);

    if (renderingNeeded (rc))
        invokeSplitRender (rc, *this);
}

void FadeInOutAudioNode::renderAdding (const AudioRenderContext& rc)
{
    if (renderingNeeded (rc))
        callRenderOver (rc);
    else
        input->renderAdding (rc);
}

AudioNode* FadeInOutAudioNode::createForEdit (Edit& edit, AudioNode* source)
{
    if (edit.masterFadeIn.get() > 0s || edit.masterFadeOut.get() > 0s)
    {
        auto length = edit.getLength();

        return new FadeInOutAudioNode (source,
                                       { 0.0, edit.masterFadeIn.get().inSeconds() },
                                       { length.inSeconds() - edit.masterFadeOut.get().inSeconds(), length.inSeconds() },
                                       edit.masterFadeInType,
                                       edit.masterFadeOutType);
    }

    return source;
}

}} // namespace tracktion { inline namespace engine
