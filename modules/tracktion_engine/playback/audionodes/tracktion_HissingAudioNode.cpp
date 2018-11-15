/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


HissingAudioNode::HissingAudioNode (AudioNode* in)
    : SingleInputAudioNode (in)
{
}

HissingAudioNode::~HissingAudioNode()
{
}

void HissingAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    SingleInputAudioNode::prepareAudioNodeToPlay (info);

    static Time appStartupTime (Time::getCurrentTime());

    const int secondsBetweenHisses = jmax (20, 35 - 2 * (int)((Time::getCurrentTime() - appStartupTime).inMinutes()));

    maxBlocksBetweenHisses = blocksBetweenHisses
        = jmax (10, (int)((info.sampleRate * (Random::getSystemRandom().nextInt (10) + secondsBetweenHisses))
                            / info.blockSizeSamples));

    blockCounter = Random::getSystemRandom().nextInt (blocksBetweenHisses);
    blocksPerHiss = jmax (2, (int) (info.sampleRate * 2.5 / info.blockSizeSamples));
    level = 0.001;
}

void HissingAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void HissingAudioNode::renderAdding (const AudioRenderContext& rc)
{
    input->renderAdding (rc);

    if (++blockCounter >= blocksBetweenHisses)
    {
        blockCounter = 0;
        level = 0.001;

        blocksBetweenHisses = jmax (10, maxBlocksBetweenHisses
                                         - Random::getSystemRandom().nextInt (maxBlocksBetweenHisses / 4));

        if (rc.bufferForMidiMessages != nullptr)
            for (int i = 1; i <= 16; ++i)
                rc.bufferForMidiMessages->addMidiMessage (MidiMessage::allNotesOff (i), 0, MidiMessageArray::notMPE);
    }

    if (blockCounter < blocksPerHiss)
    {
        if (rc.destBuffer != nullptr)
        {
            for (int j = 0; j < rc.destBuffer->getNumChannels(); ++j)
            {
                float* d = rc.destBuffer->getWritePointer (j, rc.bufferStartSample);

                for (int i = 0; i < rc.bufferNumSamples; ++i)
                {
                    double noise = 0;

                    for (int k = 4; --k >= 0;)
                        noise += random.nextDouble();

                    *d++ += (float) (level * noise * 0.2);

                    const double hissLevel = 0.17;

                    if (level < hissLevel)
                        level *= 1.000025;
                }
            }
        }
    }
}
