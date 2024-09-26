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

class PhysicalMidiInputDevice  : public MidiInputDevice
{
public:
    PhysicalMidiInputDevice (Engine&, juce::MidiDeviceInfo);
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
    void handleIncomingMidiMessage (const juce::MidiMessage&, MPESourceID) override;

    void loadProps() override;
    void saveProps() override;

    DeviceType getDeviceType() const override       { return InputDevice::physicalMidiDevice; }

    bool isTakingControllerMessages = true;

    class Listener
    {
    public:
        virtual ~Listener() {}

        virtual void handleIncomingMidiMessage (const juce::MidiMessage&) {}
    };

    void addListener (Listener* l)
    {
        const std::scoped_lock sl (listenerLock);
        listeners.add (l);
    }

    void removeListener (Listener* l)
    {
        const std::scoped_lock sl (listenerLock);
        listeners.remove (l);
    }

    juce::String openDevice() override;
    void closeDevice() override;

private:
    void handleIncomingMidiMessageInt (const juce::MidiMessage&, MPESourceID);

    friend struct PhysicalMidiInputDeviceInstance;
    juce::MidiDeviceInfo deviceInfo;
    std::unique_ptr<juce::MidiInput> inputDevice;

    std::unique_ptr<MidiControllerParser> controllerParser;
    ExternalController* externalController = nullptr;
    bool isReadingMidiTimecode = false, isAcceptingMMC = false, ignoreHours = false;

    bool tryToSendTimecode (const juce::MidiMessage&);

    ActiveNoteList activeNotes;

    RealTimeSpinLock listenerLock;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhysicalMidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
