/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


class VcaAutomatableParameter : public AutomatableParameter
{
public:
    VcaAutomatableParameter (const String& xmlTag, const String& name,
                             Plugin& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~VcaAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override
    {
        return Decibels::toString (volumeFaderPositionToDB (value) + 0.001);
    }

    float stringToValue (const String& str) override
    {
        return decibelsToVolumeFaderPosition (dbStringToDb (str));
    }
};

//==============================================================================
VCAPlugin::VCAPlugin (PluginCreationInfo info) : Plugin (info)
{
    addAutomatableParameter (volParam = new VcaAutomatableParameter ("vca", TRANS("VCA"), *this, { 0.0f, 1.0f }));

    volumeValue.referTo (state, IDs::volume, getUndoManager(), decibelsToVolumeFaderPosition (0.0f));
    volParam->attachToCurrentValue (volumeValue);
}

VCAPlugin::~VCAPlugin()
{
    notifyListenersOfDeletion();

    volParam->detachFromCurrentValue();
}

ValueTree VCAPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* VCAPlugin::xmlTypeName = "vca";

void VCAPlugin::initialise (const PlaybackInitialisationInfo&) {}
void VCAPlugin::deinitialise() {}
void VCAPlugin::applyToBuffer (const AudioRenderContext&) {}

float VCAPlugin::getSliderPos() const
{
    return volParam->getCurrentValue();
}

void VCAPlugin::setVolumeDb (float dB)
{
    setSliderPos (decibelsToVolumeFaderPosition (dB));
}

float VCAPlugin::getVolumeDb() const
{
    return volumeFaderPositionToDB (volParam->getCurrentValue());
}

void VCAPlugin::setSliderPos (float newV)
{
    volParam->setParameter (jlimit (0.0f, 1.0f, newV), sendNotification);
}

void VCAPlugin::muteOrUnmute()
{
    if (getVolumeDb() > -90.0f)
    {
        lastVolumeBeforeMute = getVolumeDb();
        setVolumeDb (lastVolumeBeforeMute - 0.01f); // needed so that automation is recorded correctly
        setVolumeDb (-100.0f);
    }
    else
    {
        if (lastVolumeBeforeMute < -100.0f)
            lastVolumeBeforeMute = 0.0f;

        setVolumeDb (getVolumeDb() + 0.01f); // needed so that automation is recorded correctly
        setVolumeDb (lastVolumeBeforeMute);
    }
}

float VCAPlugin::updateAutomationStreamAndGetVolumeDb (double time)
{
    if (isAutomationNeeded())
    {
        updateParameterStreams (time);
        updateLastPlaybackTime();
    }

    return getVolumeDb();
}

bool VCAPlugin::canBeMoved()
{
    if (auto ft = dynamic_cast<FolderTrack*> (getOwnerTrack()))
        return ft->isSubmixFolder();

    return false;
}

void VCAPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &volumeValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
}
