/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


BufferingAudioNode::BufferingAudioNode (AudioNode* input, int bufferSize_)
    : SingleInputAudioNode (input), bufferSize (bufferSize_), numSamplesLeft (0),
      sampleRate (44100.0), tempBuffer (2, 0)
{
}

BufferingAudioNode::~BufferingAudioNode()
{
}

void BufferingAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    SingleInputAudioNode::getAudioNodeProperties (info);

    tempBuffer.setSize (jmax (1, info.numberOfChannels), tempBuffer.getNumSamples());
}

void BufferingAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    if (input != nullptr)
    {
        int sizeToUse = info.blockSizeSamples;

        while (sizeToUse < bufferSize)
            sizeToUse += info.blockSizeSamples;

        tempBuffer.setSize (tempBuffer.getNumChannels(), sizeToUse);
        sampleRate = info.sampleRate;
        numSamplesLeft = 0;

        PlaybackInitialisationInfo info2 = info;
        info2.blockSizeSamples = sizeToUse;
        SingleInputAudioNode::prepareAudioNodeToPlay (info2);
    }
}

void BufferingAudioNode::prepareForNextBlock (const AudioRenderContext&)
{
    // override this so our base class doesn't call prepareForNextBlock before each input block
}

void BufferingAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void BufferingAudioNode::renderAdding (const AudioRenderContext& rc)
{
    if (input == nullptr)
        return;

    // if we've jumped around or run out of buffer we need to re-fill it
    if (! rc.isContiguousWithPreviousBlock() || numSamplesLeft < rc.bufferNumSamples)
    {
        jassert (numSamplesLeft == 0); // this should always get to empty
        jassert (sampleRate > 0.0);

        AudioRenderContext rc2 (getTempContext (rc));
        rc2.clearAll();

        numSamplesLeft = tempBuffer.getNumSamples();

        input->prepareForNextBlock (rc2);
        input->renderAdding (rc2);
    }

    // now copy the audio and MIDI data into the calling context
    const int startSample = tempBuffer.getNumSamples() - numSamplesLeft;
    const double timeOffset = startSample / sampleRate;

    if (auto ab = rc.destBuffer)
        for (int i = jmin (ab->getNumChannels(), tempBuffer.getNumChannels()); --i >= 0;)
            ab->addFrom (i, rc.bufferStartSample, tempBuffer, i, startSample, rc.bufferNumSamples);

    if (rc.bufferForMidiMessages != nullptr && tempMidiBuffer.isNotEmpty())
    {
        auto blockRange = rc.streamTime.movedToStartAt (timeOffset);

        int num = 0;

        for (auto& m : tempMidiBuffer)
        {
            if (m.getTimeStamp() > blockRange.getEnd())
                break;

            ++num;
        }

        rc.bufferForMidiMessages->mergeFromAndClearWithOffsetAndLimit (tempMidiBuffer, -timeOffset, num);
    }

    numSamplesLeft -= rc.bufferNumSamples;
}

AudioRenderContext BufferingAudioNode::getTempContext (const AudioRenderContext& rc)
{
    AudioRenderContext rc2 (rc);
    rc2.destBuffer = &tempBuffer;
    rc2.bufferStartSample = 0;
    rc2.bufferForMidiMessages = &tempMidiBuffer;
    rc2.bufferNumSamples = tempBuffer.getNumSamples();
    rc2.streamTime = rc2.streamTime.withLength (rc2.bufferNumSamples / sampleRate);

    return rc2;
}
