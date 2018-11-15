/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class PhysicalMidiInputDevice  : public MidiInputDevice
{
public:
    PhysicalMidiInputDevice (Engine&, const juce::String& name, int deviceIndex);
    ~PhysicalMidiInputDevice();

    InputDeviceInstance* createInstance (EditPlaybackContext&) override;

    void setReadingMidiTimecode (bool);
    void setIgnoresHours (bool);
    bool isIgnoringHours() const noexcept           { return ignoreHours; }

    void setAcceptingMMC (bool);
    void setReadingControllerMessages (bool);
    bool isAvailableToEdit() const override;

    /** sets the external controller to send the messages to. */
    void setExternalController (ExternalController*);
    void removeExternalController (ExternalController*);

    bool isUsedForExternalControl() const           { return externalController != nullptr; }

    void handleIncomingMidiMessage (const juce::MidiMessage&) override;

    void loadProps() override;
    void saveProps() override;

    DeviceType getDeviceType() const override       { return InputDevice::physicalMidiDevice; }

    bool isTakingControllerMessages = true;

protected:
    juce::String openDevice() override;
    void closeDevice() override;

private:
    friend struct PhysicalMidiInputDeviceInstance;
    int deviceIndex = 0;
    juce::ScopedPointer<juce::MidiInput> inputDevice;

    juce::ScopedPointer<MidiControllerParser> controllerParser;
    ExternalController* externalController = nullptr;
    bool isReadingMidiTimecode = false, isAcceptingMMC = false, ignoreHours = false;

    bool tryToSendTimecode (const juce::MidiMessage&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhysicalMidiInputDevice)
};

} // namespace tracktion_engine
