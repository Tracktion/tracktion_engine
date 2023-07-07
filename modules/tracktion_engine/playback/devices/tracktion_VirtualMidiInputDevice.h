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

    void handleMessageFromPhysicalDevice (MidiInputDevice*, const juce::MidiMessage&);
    static void broadcastMessageToAllVirtualDevices (MidiInputDevice*, const juce::MidiMessage&);

    DeviceType getDeviceType() const override      { return deviceType; }

    static void refreshDeviceNames (Engine&);

    bool useAllInputs = false;
    juce::StringArray inputDevices;

private:
    juce::String openDevice() override;
    void closeDevice() override;
    DeviceType deviceType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
