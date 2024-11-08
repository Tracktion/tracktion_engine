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

class VirtualMidiInputDevice  : public MidiInputDevice
{
public:
    VirtualMidiInputDevice (Engine&, juce::String name, DeviceType, juce::String deviceID, bool isAllMIDIIns);
    ~VirtualMidiInputDevice() override;

    InputDeviceInstance* createInstance (EditPlaybackContext&) override;

    using MidiInputDevice::handleIncomingMidiMessage;
    void handleIncomingMidiMessage (const juce::MidiMessage&, MPESourceID) override;
    juce::String getSelectableDescription() override;

    void setEnabled (bool) override;
    void loadProps() override;
    void saveProps() override;

    juce::StringArray getMIDIInputSourceDevices() const         { return inputDeviceIDs; }
    void setMIDIInputSourceDevices (const juce::StringArray deviceIDs);
    void toggleMIDIInputSourceDevice (const juce::String& deviceID);

    void handleMessageFromPhysicalDevice (PhysicalMidiInputDevice&, const juce::MidiMessage&);

    DeviceType getDeviceType() const override      { return deviceType; }

    const bool useAllInputs = false;

private:
    juce::String openDevice() override;
    void closeDevice() override;
    DeviceType deviceType;
    juce::StringArray inputDeviceIDs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
