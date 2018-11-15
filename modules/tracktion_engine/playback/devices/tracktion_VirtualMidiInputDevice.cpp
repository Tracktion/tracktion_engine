/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct VirtualMidiInputDeviceInstance  : public MidiInputDeviceInstanceBase
{
    VirtualMidiInputDeviceInstance (VirtualMidiInputDevice& d, EditPlaybackContext& c)
        : MidiInputDeviceInstanceBase (d, c)
    {
    }

    bool startRecording() override
    {
        if (! recording)
        {
            getVirtualMidiInput().masterTimeUpdate (startTime);
            recording = true;
        }

        return recording;
    }

    VirtualMidiInputDevice& getVirtualMidiInput() const   { return static_cast<VirtualMidiInputDevice&> (owner); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDeviceInstance)
};

Array<VirtualMidiInputDevice*, CriticalSection> virtualMidiDevices;

//==============================================================================
VirtualMidiInputDevice::VirtualMidiInputDevice (Engine& e, const String& name, DeviceType devType)
    : MidiInputDevice (e, devType == trackMidiDevice ? TRANS("Track MIDI Input")
                                                     : TRANS("Virtual MIDI Input"), name),
      deviceType (devType)
{
    loadProps();

    if (deviceType == MidiInputDevice::virtualMidiDevice)
        virtualMidiDevices.add (this);
}

VirtualMidiInputDevice::~VirtualMidiInputDevice()
{
    notifyListenersOfDeletion();

    if (deviceType == MidiInputDevice::virtualMidiDevice)
        virtualMidiDevices.removeAllInstancesOf (this);

    closeDevice();
}

InputDeviceInstance* VirtualMidiInputDevice::createInstance (EditPlaybackContext& c)
{
    if (! isTrackDevice() && retrospectiveBuffer == nullptr)
        retrospectiveBuffer = new RetrospectiveMidiBuffer (c.edit.engine);

    return new VirtualMidiInputDeviceInstance (*this, c);
}

String VirtualMidiInputDevice::openDevice()
{
    return {};
}

void VirtualMidiInputDevice::closeDevice()
{
}

void VirtualMidiInputDevice::setEnabled (bool b)
{
    if (b && isTrackDevice())
        loadProps();

    MidiInputDevice::setEnabled (b);
}

void VirtualMidiInputDevice::loadProps()
{
    const String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::virtualmidiin, propName);

    if (! isTrackDevice() && n != nullptr)
        inputDevices.addTokens (n->getStringAttribute ("inputDevices"), ";", {});

    MidiInputDevice::loadProps (n.get());
}

void VirtualMidiInputDevice::saveProps()
{
    XmlElement n ("SETTINGS");

    n.setAttribute ("inputDevices", inputDevices.joinIntoString (";"));
    MidiInputDevice::saveProps (n);

    const String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::virtualmidiin, propName, n);
}

void VirtualMidiInputDevice::handleIncomingMidiMessage (const MidiMessage& m)
{
    if (! (m.isActiveSense() || disallowedChannels [m.getChannel() - 1]))
    {
        MidiMessage message (m);

        if (m.getTimeStamp() == 0 || ! engine.getEngineBehaviour().isMidiDriverUsedForIncommingMessageTiming())
            message.setTimeStamp (Time::getMillisecondCounterHiRes() * 0.001);

        message.addToTimeStamp (adjustSecs);

        if (! retrospectiveRecordLock && retrospectiveBuffer != nullptr)
            retrospectiveBuffer->addMessage (message, adjustSecs);

        sendNoteOnToMidiKeyListeners (message);

        sendMessageToInstances (message);
    }
}

void VirtualMidiInputDevice::handleMessageFromPhysicalDevice (MidiInputDevice* dev, const MidiMessage& m)
{
    if (inputDevices.contains (dev->getName()))
        handleIncomingMidiMessage (m);
}

void VirtualMidiInputDevice::broadcastMessageToAllVirtualDevices (MidiInputDevice* dev, const juce::MidiMessage& m)
{
    for (auto d : virtualMidiDevices)
        d->handleMessageFromPhysicalDevice (dev, m);
}

void VirtualMidiInputDevice::refreshDeviceNames (Engine& e)
{
    String names;

    for (auto d : virtualMidiDevices)
        names += d->getName() + ";";

    e.getPropertyStorage().setProperty (SettingID::virtualmididevices, names);
}

String VirtualMidiInputDevice::getSelectableDescription()
{
    if (getDeviceType() == trackMidiDevice)
        return getAlias() + " (" + getType() + ")";

    return MidiInputDevice::getSelectableDescription();
}
