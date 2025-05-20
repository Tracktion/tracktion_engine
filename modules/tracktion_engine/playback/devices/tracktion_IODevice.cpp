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

IODevice::IODevice (Engine& e, juce::String n, juce::String idToUse)
   : engine (e), deviceID (std::move (idToUse)), name (std::move (n))
{
    alias = engine.getPropertyStorage().getPropertyItem (SettingID::invalid, getAliasPropName());
}

IODevice::~IODevice()
{
}

juce::String IODevice::getAliasPropName() const
{
    return "devicealias_" + deviceID;
}

juce::String IODevice::getAliasOrName() const
{
    return alias.isNotEmpty() ? alias : name;
}

juce::String IODevice::getAliasIfSet() const
{
    return alias;
}

void IODevice::setAlias (const juce::String& a)
{
    auto newAlias = a.substring (0, 50).trim();

    if (newAlias == getName())
        newAlias = {};

    if (alias != newAlias)
    {
        alias = newAlias;

        if (alias.isNotEmpty() && ! isTrackDevice())
            engine.getPropertyStorage().setPropertyItem (SettingID::invalid, getAliasPropName(), alias);
        else
            engine.getPropertyStorage().removePropertyItem (SettingID::invalid, getAliasPropName());

        changed();
    }
}

juce::String IODevice::getSelectableDescription()
{
    return getAliasOrName() + " (" + TRANS(getDeviceTypeDescription()) + ")";
}

bool IODevice::isEnabled() const
{
    return enabled;
}


}} // namespace tracktion { inline namespace engine
