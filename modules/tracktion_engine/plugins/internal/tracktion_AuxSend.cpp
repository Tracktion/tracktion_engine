/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AuxSendPlugin::AuxSendPlugin (PluginCreationInfo info) : Plugin (info)
{
    auto um = getUndoManager();
    busNumber.referTo (state, IDs::busNum, um);
    gainLevel.referTo (state, IDs::auxSendSliderPos, um, decibelsToVolumeFaderPosition (0.0f));

    gain = addParam ("send level", TRANS("Send level"), { 0.0f, 1.0f },
                     [] (float value)       { return Decibels::toString (volumeFaderPositionToDB (value)); },
                     [] (const String& s)   { return decibelsToVolumeFaderPosition (dbStringToDb (s)); });

    gain->attachToCurrentValue (gainLevel);

    quickParamName = "send level";
}

AuxSendPlugin::~AuxSendPlugin()
{
    notifyListenersOfDeletion();
    gain->detachFromCurrentValue();
}

const char* AuxSendPlugin::xmlTypeName = "auxsend";

String AuxSendPlugin::getName()
{
    String nm (edit.getAuxBusName (busNumber));

    if (nm.isNotEmpty())
        return "S:" + nm;

    return TRANS("Aux Send") + " #" + String (busNumber + 1);
}

String AuxSendPlugin::getShortName (int)
{
    auto nm = edit.getAuxBusName (busNumber);

    if (nm.isNotEmpty())
        return "S:" + nm;

    return TRANS("Send") + ":" + String (busNumber + 1);
}

void AuxSendPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    delayBuffer.setSize (2, info.blockSizeSamples, false);
    delayBuffer.clear();
    lastGain = volumeFaderPositionToGain (gain->getCurrentValue());
    latencySeconds = info.blockSizeSamples / info.sampleRate;

    initialiseWithoutStopping (info);
}

void AuxSendPlugin::initialiseWithoutStopping (const PlaybackInitialisationInfo& info)
{
    juce::Array<AuxReturnPlugin*> result;

    for (auto node : *info.rootNodes)
        node->visitNodes ([&] (AudioNode& node)
                          {
                              if (auto ar = dynamic_cast<AuxReturnPlugin*> (node.getPlugin().get()))
                                  result.add (ar);
                          });


    allAuxReturns.swapWith (result);
}

void AuxSendPlugin::deinitialise()
{
    allAuxReturns.clear();
    delayBuffer.setSize (2, 32, false);
}

void AuxSendPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    auto gainScalar = volumeFaderPositionToGain (gain->getCurrentValue());

    for (auto f : allAuxReturns)
    {
        if (f->busNumber == busNumber)
        {
            if (lastGain != gainScalar)
            {
                f->applyAudioFromSend (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, lastGain, gainScalar);
                lastGain = gainScalar;
            }
            else
            {
                f->applyAudioFromSend (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, gainScalar);
            }
        }
    }

    delayBuffer.setSize (jmax (fc.destBuffer->getNumChannels(), delayBuffer.getNumChannels()),
                         jmax (fc.bufferNumSamples, delayBuffer.getNumSamples()),
                         true);

    for (int i = jmin (fc.destBuffer->getNumChannels(), delayBuffer.getNumChannels()); --i >= 0;)
    {
        auto delay = delayBuffer.getWritePointer (i);
        auto dest = fc.destBuffer->getWritePointer (i, fc.bufferStartSample);

        for (int j = 0; j < fc.bufferNumSamples; ++j)
        {
            auto v = dest[j];
            dest[j] = delay[j];
            delay[j] = v;
        }
    }
}

String AuxSendPlugin::getBusName()
{
    auto busName = edit.getAuxBusName (busNumber);

    if (busName.isNotEmpty())
        return busName;

    return getDefaultBusName (busNumber);
}

void AuxSendPlugin::setGainDb (float newDb)
{
    float newPos = decibelsToVolumeFaderPosition (newDb);

    if (gain->getCurrentValue() != newPos)
    {
        gain->setParameter (newPos, sendNotification);
        changed();
    }
}

void AuxSendPlugin::setMute (bool b)
{
    if (b)
    {
        lastVolumeBeforeMute = getGainDb();
        setGainDb (lastVolumeBeforeMute - 0.01f); // needed so that automation is recorded correctly
        setGainDb (-100.0f);
    }
    else
    {
        if (lastVolumeBeforeMute < -100.0f)
            lastVolumeBeforeMute = 0.0f;

        setGainDb (getGainDb() + 0.01f); // needed so that automation is recorded correctly
        setGainDb (lastVolumeBeforeMute);
    }
}

bool AuxSendPlugin::isMute()
{
    return getGainDb() <= -90.0f;
}

String AuxSendPlugin::getDefaultBusName (int index)
{
    return "Bus #" + String (index + 1);
}

StringArray AuxSendPlugin::getBusNames (Edit& edit)
{
    StringArray buses;

    for (int i = 0; i < maxNumBusses; ++i)
    {
        auto nm = getDefaultBusName (i);

        if (edit.getAuxBusName (i).isNotEmpty())
            nm << " (" << edit.getAuxBusName (i) << ")";

        buses.add (nm);
    }

    return buses;
}

void AuxSendPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &gainLevel, nullptr };
    CachedValue<int>* cvsInt[]      = { &busNumber, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
}
