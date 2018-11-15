/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AuxReturnPlugin::AuxReturnPlugin (PluginCreationInfo info)  : Plugin (info)
{
    busNumber.referTo (state, IDs::busNum, getUndoManager());
}

AuxReturnPlugin::~AuxReturnPlugin()
{
    notifyListenersOfDeletion();
}

const char* AuxReturnPlugin::xmlTypeName = "auxreturn";

String AuxReturnPlugin::getName()
{
    String nm (edit.getAuxBusName (busNumber));

    if (nm.isNotEmpty())
        return "R:" + nm;

    return TRANS("Aux Return") + " #" + String (busNumber + 1);
}

String AuxReturnPlugin::getShortName (int)
{
    String nm (edit.getAuxBusName (busNumber));

    if (nm.isNotEmpty())
        return "R:" + nm;

    return "Ret:" + String (busNumber + 1);
}

void AuxReturnPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    previousBuffer.reset (new juce::AudioBuffer<float> (2, info.blockSizeSamples * 2));
    currentBuffer.reset (new juce::AudioBuffer<float> (2, info.blockSizeSamples * 2));
    samplesInPreviousBuffer = 0;
    samplesInCurrentBuffer = 0;

    initialised = true;
}

void AuxReturnPlugin::deinitialise()
{
    initialised = false;

    previousBuffer = nullptr;
    currentBuffer = nullptr;
}

void AuxReturnPlugin::prepareForNextBlock (const AudioRenderContext&)
{
    const ScopedLock sl (addingAudioLock);

    std::swap (previousBuffer, currentBuffer);
    samplesInPreviousBuffer = samplesInCurrentBuffer;
    samplesInCurrentBuffer = 0;
}

void AuxReturnPlugin::applyAudioFromSend (const juce::AudioBuffer<float>& buffer,
                                          int startSample, int numSamples,
                                          float gain1, float gain2)
{
    const ScopedLock sl (addingAudioLock);

    if (initialised)
    {
        const int numChansToUse = jmin (currentBuffer->getNumChannels(), buffer.getNumChannels());

        if (samplesInCurrentBuffer == 0)
        {
            samplesInCurrentBuffer = numSamples;

            if (currentBuffer->getNumSamples() < numSamples)
                currentBuffer->setSize (numChansToUse, numSamples, false);

            int iSrcChan = numChansToUse - 1;

            for (int iDstChan = currentBuffer->getNumChannels(); --iDstChan >= 0;)
            {
                currentBuffer->copyFrom (iDstChan, 0, buffer, iSrcChan, startSample, numSamples);

                if (iSrcChan > 0)
                    --iSrcChan;
            }

            AudioFadeCurve::applyCrossfadeSection (*currentBuffer, 0, numSamples, AudioFadeCurve::linear, gain1, gain2);
        }
        else
        {
            // very nasty if this gets called with different block sizes..
            jassert (numSamples == samplesInCurrentBuffer);

            numSamples = jmin (numSamples, samplesInCurrentBuffer);

            int iSrcChan = numChansToUse - 1;

            for (int iDstChan = currentBuffer->getNumChannels(); --iDstChan >= 0;)
            {
                AudioFadeCurve::addWithCrossfade (*currentBuffer, buffer,
                                                  iDstChan, 0, iSrcChan, startSample, numSamples,
                                                  AudioFadeCurve::linear, gain1, gain2);

                if (iSrcChan > 0)
                    --iSrcChan;
            }
        }
    }
}

void AuxReturnPlugin::applyAudioFromSend (const juce::AudioBuffer<float>& buffer,
                                          int startSample, int numSamples,
                                          const float gain)
{
    const ScopedLock sl (addingAudioLock);

    if (initialised)
    {
        const int numChansToUse = jmin (currentBuffer->getNumChannels(), buffer.getNumChannels());

        if (samplesInCurrentBuffer == 0)
        {
            samplesInCurrentBuffer = numSamples;

            if (currentBuffer->getNumSamples() < numSamples)
                currentBuffer->setSize (numChansToUse, numSamples, false);

            int iSrcChan = numChansToUse - 1;

            for (int iDstChan = currentBuffer->getNumChannels(); --iDstChan >= 0;)
            {
                currentBuffer->copyFrom (iDstChan, 0, buffer, iSrcChan, startSample, numSamples);

                if (iSrcChan > 0)
                    --iSrcChan;
            }

            currentBuffer->applyGain (0, numSamples, gain);
        }
        else
        {
            // very nasty if this gets called with different block sizes..
            jassert (numSamples == samplesInCurrentBuffer);

            numSamples = jmin (numSamples, samplesInCurrentBuffer);

            int iSrcChan = numChansToUse - 1;

            for (int iDstChan = currentBuffer->getNumChannels(); --iDstChan >= 0;)
            {
                currentBuffer->addFrom (iDstChan, 0, buffer, iSrcChan, startSample, numSamples, gain);

                if (iSrcChan > 0)
                    --iSrcChan;
            }
        }
    }
}

void AuxReturnPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    if (samplesInPreviousBuffer > 0)
    {
        const int sampsToCopy = jmin (fc.bufferNumSamples, samplesInPreviousBuffer);

        if (fc.destBuffer->getNumChannels() == 1 && previousBuffer->getNumChannels() >= 1)
        {
            // stereo -> mono
            for (int i = previousBuffer->getNumChannels(); --i >= 0;)
            {
                // stereo -> mono
                fc.destBuffer->addFrom (0, fc.bufferStartSample, *previousBuffer, i, 0, sampsToCopy);
            }
        }
        else
        {
            for (int i = jmin (previousBuffer->getNumChannels(),
                               fc.destBuffer->getNumChannels()); --i >= 0;)
            {
                // stereo -> stereo
                // mono   -> mono
                // mono   -> stereo
                fc.destBuffer->addFrom (i, fc.bufferStartSample, *previousBuffer, i, 0, sampsToCopy);
            }
        }
    }
}

void AuxReturnPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<int>* cvsInt[] = { &busNumber, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
}
