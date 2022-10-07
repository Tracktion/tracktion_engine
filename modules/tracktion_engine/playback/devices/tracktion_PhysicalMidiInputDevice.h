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

class PhysicalMidiInputDevice  : public MidiInputDevice
{
public:
    PhysicalMidiInputDevice (Engine&, const juce::String& name, int deviceIndex);
    ~PhysicalMidiInputDevice() override;

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

    using MidiInputDevice::handleIncomingMidiMessage;
    void handleIncomingMidiMessage (const juce::MidiMessage&) override;

    void loadProps() override;
    void saveProps() override;

    DeviceType getDeviceType() const override       { return InputDevice::physicalMidiDevice; }

    bool isTakingControllerMessages = true;

protected:
    juce::String openDevice() override;
    void closeDevice() override;

private:
    void handleIncomingMidiMessageInt (const juce::MidiMessage&);

    friend struct PhysicalMidiInputDeviceInstance;
    int deviceIndex = 0;
    std::unique_ptr<juce::MidiInput> inputDevice;

    std::unique_ptr<MidiControllerParser> controllerParser;
    ExternalController* externalController = nullptr;
    bool isReadingMidiTimecode = false, isAcceptingMMC = false, ignoreHours = false;

    bool tryToSendTimecode (const juce::MidiMessage&);

    ActiveNoteList activeNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhysicalMidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
