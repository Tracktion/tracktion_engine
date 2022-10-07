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

class VcaAutomatableParameter : public AutomatableParameter
{
public:
    VcaAutomatableParameter (const juce::String& xmlTag, const juce::String& name,
                             Plugin& owner, juce::Range<float> valueRangeToUse)
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse)
    {
    }

    ~VcaAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String valueToString (float value) override
    {
        return juce::Decibels::toString (volumeFaderPositionToDB (value) + 0.001);
    }

    float stringToValue (const juce::String& str) override
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

juce::ValueTree VCAPlugin::create()
{
    juce::ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* VCAPlugin::xmlTypeName = "vca";

void VCAPlugin::initialise (const PluginInitialisationInfo&) {}
void VCAPlugin::deinitialise() {}
void VCAPlugin::applyToBuffer (const PluginRenderContext&) {}

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
    volParam->setParameter (juce::jlimit (0.0f, 1.0f, newV), juce::sendNotification);
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

float VCAPlugin::updateAutomationStreamAndGetVolumeDb (TimePosition time)
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

void VCAPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[]  = { &volumeValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}} // namespace tracktion { inline namespace engine
