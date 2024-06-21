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

class VirtualMidiInputDevice  : public MidiInputDevice
{
public:
    VirtualMidiInputDevice (Engine&, const juce::String& name, DeviceType);
    ~VirtualMidiInputDevice() override;

    InputDeviceInstance* createInstance (EditPlaybackContext&) override;

    using MidiInputDevice::handleIncomingMidiMessage;
    void handleIncomingMidiMessage (const juce::MidiMessage&) override;
    juce::String getSelectableDescription() override;

    void setEnabled (bool) override;
    void loadProps() override;
    void saveProps() override;

    juce::StringArray getMIDIInputSourceDevices() const         { return inputDeviceIDs; }
    void setMIDIInputSourceDevices (const juce::StringArray deviceIDs);
    void toggleMIDIInputSourceDevice (const juce::String& deviceID);

    void handleMessageFromPhysicalDevice (MidiInputDevice&, const juce::MidiMessage&);

    DeviceType getDeviceType() const override      { return deviceType; }

    bool useAllInputs = false;

    static constexpr const char* allMidiInsName = "All MIDI Ins";

private:
    juce::String openDevice() override;
    void closeDevice() override;
    DeviceType deviceType;
    juce::StringArray inputDeviceIDs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
