/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


OutputDevice::OutputDevice (Engine& e, const String& t, const String& n)
   : engine (e), type (t), name (n)
{
    alias = engine.getPropertyStorage().getPropertyItem (SettingID::invalid, getAliasPropName());
    defaultAlias = n;
}

OutputDevice::~OutputDevice()
{
}

String OutputDevice::getAliasPropName() const
{
    return type + "out_" + name + "_alias";
}

void OutputDevice::initialiseDefaultAlias()
{
    defaultAlias = getName();

    if (defaultAlias == DeviceManager::getDefaultAudioDeviceName (false))
        defaultAlias = DeviceManager::getDefaultAudioDeviceName (true);

    if (defaultAlias == DeviceManager::getDefaultMidiDeviceName (false))
        defaultAlias = DeviceManager::getDefaultMidiDeviceName (true);

    defaultAlias = findDefaultAliasNameNotClashingWithInputDevices (engine, isMidi(), getName(), defaultAlias);
}

String OutputDevice::getName() const
{
    return name;
}

String OutputDevice::getAlias() const
{
    if (alias.isNotEmpty())
        return alias;

    return defaultAlias;
}

void OutputDevice::setAlias (const String& a)
{
    if (alias != a)
    {
        if (a != defaultAlias)
        {
            alias = a.substring (0, 20);
            engine.getPropertyStorage().setPropertyItem (SettingID::invalid, getAliasPropName(), a);
        }
        else
        {
            alias = {};
            engine.getPropertyStorage().removePropertyItem (SettingID::invalid, getAliasPropName());
        }

        changed();
    }
}

String OutputDevice::getSelectableDescription()
{
    return name + " (" + type + ")";
}

String OutputDevice::getDeviceID() const
{
    auto n = getName();

    if (isMidi())
        n += TRANS("MIDI");
    else
        n += engine.getDeviceManager().deviceManager.getCurrentAudioDeviceType();

    return String::toHexString (n.hashCode());
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

AudioNode* OutputDeviceInstance::replaceAudioNode (AudioNode* node)
{
    const ScopedLock sl (audioNodeLock);

    auto oldOne = audioNode.release();
    audioNode = node;
    return oldOne;
}
