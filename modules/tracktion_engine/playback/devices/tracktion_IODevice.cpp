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

juce::String IODevice::getAlias() const
{
    if (alias.isNotEmpty())
        return alias;

    return getName();
}

void IODevice::setAlias (const juce::String& a)
{
    if (alias != a)
    {
        alias = a.substring (0, 40).trim();

        if (alias == getName())
            alias = {};

        if (! isTrackDevice())
        {
            if (alias.isNotEmpty())
                engine.getPropertyStorage().setPropertyItem (SettingID::invalid, getAliasPropName(), alias);
            else
                engine.getPropertyStorage().removePropertyItem (SettingID::invalid, getAliasPropName());
        }

        changed();
    }
}

juce::String IODevice::getSelectableDescription()
{
    return name + " (" + TRANS(getDeviceTypeDescription()) + ")";
}

bool IODevice::isEnabled() const
{
    return enabled;
}



}} // namespace tracktion { inline namespace engine
