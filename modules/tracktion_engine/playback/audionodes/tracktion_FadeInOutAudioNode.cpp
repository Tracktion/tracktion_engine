/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


FadeInOutAudioNode::FadeInOutAudioNode (AudioNode* inp,
                                        EditTimeRange in, EditTimeRange out,
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

static int timeToSample (const AudioRenderContext& rc, EditTimeRange editTime, double t)
{
    return (int) (rc.bufferNumSamples * (t - editTime.getStart()) / editTime.getLength() + 0.5);
}

void FadeInOutAudioNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
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
            alpha2 = jmax (0.0, (editTime.getEnd() - fadeIn.getStart()) / fadeIn.getLength());
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
                                                   jlimit (0.0f, 1.0f, (float) (1.0 - alpha1)),
                                                   jlimit (0.0f, 1.0f, (float) (1.0 - alpha2)));
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

AudioNode* FadeInOutAudioNode::createForEdit (Edit& edit, AudioNode* input)
{
    if (edit.masterFadeIn > 0 || edit.masterFadeOut > 0)
    {
        const double length = edit.getLength();

        return new FadeInOutAudioNode (input,
                                       { 0.0, edit.masterFadeIn },
                                       { length - edit.masterFadeOut, length },
                                       edit.masterFadeInType,
                                       edit.masterFadeOutType);
    }

    return input;
}
