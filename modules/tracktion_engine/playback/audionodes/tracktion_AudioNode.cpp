/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AudioNode::AudioNode()
{
}

AudioNode::~AudioNode()
{
}

void AudioNode::callRenderAdding (const AudioRenderContext& rc)
{
    rc.clearAll();
    renderAdding (rc);
}

void AudioNode::callRenderOverForMidi (const AudioRenderContext& rc)
{
    if (rc.bufferForMidiMessages == nullptr || rc.bufferForMidiMessages->isEmpty())
    {
        renderOver (rc);
    }
    else
    {
        MidiMessageArray temp;

        AudioRenderContext rc2 (rc);
        rc2.bufferForMidiMessages = &temp;

        renderOver (rc2);

        rc.bufferForMidiMessages->mergeFromAndClear (temp);
    }
}

void AudioNode::callRenderOver (const AudioRenderContext& rc)
{
    if (rc.destBuffer != nullptr)
    {
        AudioScratchBuffer scratch (rc.destBuffer->getNumChannels(), rc.bufferNumSamples);

        AudioRenderContext rc2 (rc);
        rc2.destBuffer = &scratch.buffer;
        rc2.bufferStartSample = 0;

        callRenderOverForMidi (rc2);

        for (int i = rc.destBuffer->getNumChannels(); --i >= 0;)
            rc.destBuffer->addFrom (i, rc.bufferStartSample, *rc2.destBuffer, i, 0, rc.bufferNumSamples);
    }
    else
    {
        callRenderOverForMidi (rc);
    }
}

void AudioRenderContext::clearAudioBuffer() const noexcept
{
    if (destBuffer != nullptr)
        destBuffer->clear (bufferStartSample, bufferNumSamples);
}

void AudioRenderContext::addAntiDenormalisationNoise() const noexcept
{
    if (destBuffer != nullptr)
        tracktion_engine::addAntiDenormalisationNoise (*destBuffer, bufferStartSample, bufferNumSamples);
}

void AudioRenderContext::clearMidiBuffer() const noexcept
{
    if (bufferForMidiMessages != nullptr)
        bufferForMidiMessages->clear();
}

void AudioRenderContext::clearAll() const noexcept
{
    clearAudioBuffer();
    clearMidiBuffer();
}

PlayHead::EditTimeWindow AudioRenderContext::getEditTime() const
{
    return playhead.streamTimeToEditWindow (streamTime);
}

//==============================================================================
SingleInputAudioNode::SingleInputAudioNode (AudioNode* n)  : input (n)
{
    jassert (input != nullptr);
}

void SingleInputAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    input->getAudioNodeProperties (info);
}

void SingleInputAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    input->prepareAudioNodeToPlay (info);
}

bool SingleInputAudioNode::purgeSubNodes (bool keepAudio, bool keepMidi)
{
    return input->purgeSubNodes (keepAudio, keepMidi);
}

void SingleInputAudioNode::releaseAudioNodeResources()
{
    input->releaseAudioNodeResources();
}

void SingleInputAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);
    input->visitNodes (v);
}

Plugin::Ptr SingleInputAudioNode::getPlugin() const
{
    return input->getPlugin();
}

void SingleInputAudioNode::prepareForNextBlock (const AudioRenderContext& rc)
{
    input->prepareForNextBlock (rc);
}

bool SingleInputAudioNode::isReadyToRender()
{
    return input->isReadyToRender();
}

void SingleInputAudioNode::renderOver (const AudioRenderContext& rc)
{
    input->renderOver (rc);
}

void SingleInputAudioNode::renderAdding (const AudioRenderContext& rc)
{
    input->renderAdding (rc);
}
