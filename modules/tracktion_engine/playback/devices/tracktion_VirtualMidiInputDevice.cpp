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

struct VirtualMidiInputDeviceInstance  : public MidiInputDeviceInstanceBase
{
    VirtualMidiInputDeviceInstance (VirtualMidiInputDevice& d, EditPlaybackContext& c)
        : MidiInputDeviceInstanceBase (d, c)
    {
    }

    VirtualMidiInputDevice& getVirtualMidiInput() const   { return static_cast<VirtualMidiInputDevice&> (owner); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDeviceInstance)
};

//==============================================================================
VirtualMidiInputDevice::VirtualMidiInputDevice (Engine& e, juce::String deviceName, DeviceType devType,
                                                juce::String deviceIDToUse, bool isAllMIDIIns)
    : MidiInputDevice (e, devType == trackMidiDevice ? TRANS("Track MIDI Input")
                                                     : TRANS("Virtual MIDI Input"),
                       deviceName, deviceIDToUse),
      useAllInputs (isAllMIDIIns),
      deviceType (devType)
{
    if (isAllMIDIIns)
        defaultMonitorMode = MonitorMode::on;

    loadProps();
}

VirtualMidiInputDevice::~VirtualMidiInputDevice()
{
    notifyListenersOfDeletion();
    closeDevice();
}

InputDeviceInstance* VirtualMidiInputDevice::createInstance (EditPlaybackContext& c)
{
    if (! isTrackDevice() && retrospectiveBuffer == nullptr)
        retrospectiveBuffer = std::make_unique<RetrospectiveMidiBuffer> (c.edit.engine);

    return new VirtualMidiInputDeviceInstance (*this, c);
}

juce::String VirtualMidiInputDevice::openDevice() { return {}; }
void VirtualMidiInputDevice::closeDevice() {}

void VirtualMidiInputDevice::setEnabled (bool b)
{
    if (b && isTrackDevice())
        loadProps();

    MidiInputDevice::setEnabled (b);
}

void VirtualMidiInputDevice::setMIDIInputSourceDevices (const juce::StringArray deviceIDs)
{
    if (deviceIDs != inputDeviceIDs)
    {
        inputDeviceIDs = deviceIDs;
        changed();
        saveProps();
    }
}

void VirtualMidiInputDevice::toggleMIDIInputSourceDevice (const juce::String& deviceIDToToggle)
{
    auto devices = inputDeviceIDs;

    if (devices.contains (deviceIDToToggle))
        devices.removeString (deviceIDToToggle);
    else
        devices.add (deviceIDToToggle);

    setMIDIInputSourceDevices (devices);
}

void VirtualMidiInputDevice::loadProps()
{
    juce::String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::virtualmidiin, propName);

    if (! isTrackDevice() && n != nullptr)
    {
        inputDeviceIDs.addTokens (n->getStringAttribute ("inputDevices"), ";", {});
        useAllInputs = n->getBoolAttribute ("useAllInputs", false);
    }

    MidiInputDevice::loadMidiProps (n.get());
}

void VirtualMidiInputDevice::saveProps()
{
    juce::XmlElement n ("SETTINGS");

    n.setAttribute ("inputDevices", inputDeviceIDs.joinIntoString (";"));
    n.setAttribute ("useAllInputs", useAllInputs);
    MidiInputDevice::saveMidiProps (n);

    juce::String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::virtualmidiin, propName, n);
}

void VirtualMidiInputDevice::handleIncomingMidiMessage (const juce::MidiMessage& m)
{
    auto message = m;

    if (handleIncomingMessage (message))
        sendMessageToInstances (message);
}

void VirtualMidiInputDevice::handleMessageFromPhysicalDevice (MidiInputDevice& dev, const juce::MidiMessage& m)
{
    if (useAllInputs || inputDeviceIDs.contains (dev.getDeviceID()))
        handleIncomingMidiMessage (m);
}

juce::String VirtualMidiInputDevice::getSelectableDescription()
{
    if (getDeviceType() == trackMidiDevice)
        return getAlias() + " (" + getType() + ")";

    return MidiInputDevice::getSelectableDescription();
}

}} // namespace tracktion { inline namespace engine
