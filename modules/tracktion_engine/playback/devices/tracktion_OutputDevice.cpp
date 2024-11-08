/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

OutputDevice::OutputDevice (Engine& e, juce::String t, juce::String n, juce::String idToUse)
   : engine (e), type (t),
     deviceID ("out_" + juce::String::toHexString ((t + idToUse).hashCode())),
     name (n)
{
    alias = engine.getPropertyStorage().getPropertyItem (SettingID::invalid, getAliasPropName());
}

OutputDevice::~OutputDevice()
{
}

juce::String OutputDevice::getAliasPropName() const
{
    return type + "out_" + name + "_alias";
}

juce::String OutputDevice::getName() const
{
    return name;
}

juce::String OutputDevice::getAlias() const
{
    if (alias.isNotEmpty())
        return alias;

    return getName();
}

void OutputDevice::setAlias (const juce::String& a)
{
    if (alias != a)
    {
        alias = a.substring (0, 40).trim();

        if (alias == getName())
            alias = {};

        if (alias.isNotEmpty())
            engine.getPropertyStorage().setPropertyItem (SettingID::invalid, getAliasPropName(), alias);
        else
            engine.getPropertyStorage().removePropertyItem (SettingID::invalid, getAliasPropName());

        changed();
    }
}

juce::String OutputDevice::getSelectableDescription()
{
    return name + " (" + TRANS(type) + ")";
}

juce::String OutputDevice::getDeviceID() const
{
    auto n = getName();

    if (isMidi())
        n += TRANS("MIDI");
    else
        n += engine.getDeviceManager().deviceManager->getCurrentAudioDeviceType();

    return juce::String::toHexString (n.hashCode());
}

bool OutputDevice::isEnabled() const
{
    return enabled;
}

//==============================================================================
OutputDeviceInstance::OutputDeviceInstance (OutputDevice& d, EditPlaybackContext& c)
    : owner (d), context (c), edit (c.edit)
{
}

OutputDeviceInstance::~OutputDeviceInstance()
{
}

}} // namespace tracktion { inline namespace engine
