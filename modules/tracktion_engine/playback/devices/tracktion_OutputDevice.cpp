/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

OutputDevice::OutputDevice (Engine& e, const juce::String& t, const juce::String& n)
   : engine (e), type (t), name (n)
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
    return name + " (" + type + ")";
}

juce::String OutputDevice::getDeviceID() const
{
    auto n = getName();

    if (isMidi())
        n += "MIDI";
    else
        n += engine.getDeviceManager().deviceManager.getCurrentAudioDeviceType();

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
