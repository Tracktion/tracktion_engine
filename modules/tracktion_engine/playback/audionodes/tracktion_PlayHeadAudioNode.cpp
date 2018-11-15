/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


PlayHeadAudioNode::PlayHeadAudioNode (AudioNode* input)
   : SingleInputAudioNode (input)
{
}

PlayHeadAudioNode::~PlayHeadAudioNode()
{
}

//==============================================================================
void PlayHeadAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    SingleInputAudioNode::getAudioNodeProperties (info);

    hasAnyContent = info.hasAudio || info.hasMidi;
}

void PlayHeadAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    SingleInputAudioNode::prepareAudioNodeToPlay (info);
    isPlayHeadRunning = false;
    firstCallback = true;
}

void PlayHeadAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void PlayHeadAudioNode::renderAdding (const AudioRenderContext& rc)
{
    const bool isAudibleNow = rc.playhead.isPlaying();
    bool jumped = false;

    if (lastUserInteractionTime != rc.playhead.getLastUserInteractionTime())
    {
        lastUserInteractionTime = rc.playhead.getLastUserInteractionTime();
        jumped = ! firstCallback;
    }

    if (isAudibleNow != isPlayHeadRunning)
    {
        isPlayHeadRunning = isAudibleNow;
        jumped = jumped || (isPlayHeadRunning && ! firstCallback);
    }

    firstCallback = false;

    if (jumped && rc.bufferForMidiMessages != nullptr)
        rc.bufferForMidiMessages->isAllNotesOff = true;

    if (isAudibleNow && hasAnyContent)
    {
        if (jumped)
        {
            AudioRenderContext rc2 (rc);
            rc2.continuity = AudioRenderContext::playheadJumped;
            input->renderAdding (rc2);
        }
        else
        {
            input->renderAdding (rc);
        }
    }
}
