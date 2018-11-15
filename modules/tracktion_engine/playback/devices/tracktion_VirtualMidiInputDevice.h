/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class VirtualMidiInputDevice  : public MidiInputDevice
{
public:
    VirtualMidiInputDevice (Engine&, const juce::String& name, DeviceType);
    ~VirtualMidiInputDevice();

    InputDeviceInstance* createInstance (EditPlaybackContext&) override;
    void handleIncomingMidiMessage (const juce::MidiMessage&) override;
    juce::String getSelectableDescription() override;

    void setEnabled (bool) override;
    void loadProps() override;
    void saveProps() override;

    void handleMessageFromPhysicalDevice (MidiInputDevice*, const juce::MidiMessage&);
    static void broadcastMessageToAllVirtualDevices (MidiInputDevice*, const juce::MidiMessage&);

    DeviceType getDeviceType() const override      { return deviceType; }

    static void refreshDeviceNames (Engine&);

    juce::StringArray inputDevices;

private:
    juce::String openDevice() override;
    void closeDevice() override;
    DeviceType deviceType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualMidiInputDevice)
};

} // namespace tracktion_engine
