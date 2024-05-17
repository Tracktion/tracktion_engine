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

/** */
class MidiInputDevice : public InputDevice,
                        public juce::MidiInputCallback,
                        private juce::Timer,
                        private juce::MidiKeyboardStateListener
{
public:
    MidiInputDevice (Engine&, const juce::String& type, const juce::String& name);
    ~MidiInputDevice() override;

    virtual juce::String openDevice() = 0;
    virtual void closeDevice() = 0;

    void setEnabled (bool) override;
    bool isMidi() const override                    { return true; }

    MidiChannel getChannelToUse() const noexcept    { return channelToUse; }
    void setChannelToUse (int);

    int getProgramToUse() const noexcept            { return programToUse; }
    void setProgramToUse (int);

    void setBankToUse (int);
    int getBankToUse() const                        { return bankToUse; }

    void setChannelAllowed (int midiChannel, bool);
    bool isChannelAllowed (int midiChannel) const   { return ! disallowedChannels[midiChannel - 1]; }

    void setOverridingNoteVelocities (bool);
    bool isOverridingNoteVelocities() const         { return overrideNoteVels; }

    void setManualAdjustmentMs (double);
    double getManualAdjustmentMs() const            { return manualAdjustMs; }

    void setMinimumLengthMs (double);
    double getMinimumLengthMs() const               { return minimumLengthMs; }

    /** Returns true if the given device is an MPE device and so should always record incoming MIDI to Note Expression. */
    bool isMPEDevice() const;

    //==============================================================================
    void masterTimeUpdate (double time) override;
    void connectionStateChanged();

    //==============================================================================
    void addInstance (MidiInputDeviceInstanceBase*);
    void removeInstance (MidiInputDeviceInstanceBase*);

    virtual void loadProps() = 0;

    bool mergeRecordings = true;
    bool recordingEnabled = true;
    bool replaceExistingClips = false;
    bool recordToNoteAutomation = false;
    QuantisationType quantisation;

    enum class MergeMode { always, never, optional };

    Clip* addMidiAsTransaction (Edit&, EditItemID targetID,
                                Clip* takeClip, juce::MidiMessageSequence,
                                TimeRange markedRange, MergeMode, MidiChannel);

    juce::MidiKeyboardState keyboardState;

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    virtual void handleIncomingMidiMessage (const juce::MidiMessage&) = 0;

    RetrospectiveMidiBuffer* getRetrospectiveMidiBuffer() const     { return retrospectiveBuffer.get(); }
    void updateRetrospectiveBufferLength (double length) override;
    double getAdjustSecs() const                                    { return adjustSecs; }

    juce::Array<AudioTrack*> getDestinationTracks();

    MidiChannel getMidiChannelFor (int rawChannelNumber) const;
    MidiMessageArray::MPESourceID getMPESourceID() const            { return midiSourceID; }

    //==============================================================================
    /** Gets notified (lazily, not in real-time) when any MidiInputDevice's key's state changes. */
    struct MidiKeyChangeDispatcher
    {
        struct Listener
        {
            virtual ~Listener() {}

            /** Callback to indicate notes have been played for the given track. */
            virtual void midiKeyStateChanged (AudioTrack*, const juce::Array<int>& notesOn, const juce::Array<int>& vels, const juce::Array<int>& notesOff) = 0;
        };

        juce::ListenerList<Listener> listeners;

    private:
        friend class juce::SharedResourcePointer<MidiKeyChangeDispatcher>;
        MidiKeyChangeDispatcher() {}
    };

protected:
    class MidiEventSnifferNode;
    class NoteDispatcher;

    std::atomic<double> adjustSecs { 0 };
    double manualAdjustMs = 0;
    double minimumLengthMs = 0;
    bool overrideNoteVels = false, eventReceivedFromDevice = false;
    juce::BigInteger disallowedChannels;
    MidiChannel channelToUse;
    int programToUse = 0;
    int bankToUse = 0;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    std::unique_ptr<NoteDispatcher> noteDispatcher;
    std::vector<double> lastNoteOns;

    juce::CriticalSection noteLock;
    bool keysDown[128], keysUp[128];
    uint8_t keyDownVelocities[128];
    juce::SharedResourcePointer<MidiKeyChangeDispatcher> midiKeyChangeDispatcher;

    juce::CriticalSection instanceLock;
    juce::Array<MidiInputDeviceInstanceBase*> instances;
    std::unique_ptr<RetrospectiveMidiBuffer> retrospectiveBuffer;

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    void timerCallback() override;

    void sendNoteOnToMidiKeyListeners (juce::MidiMessage&);

    void loadMidiProps (const juce::XmlElement*);
    void saveMidiProps (juce::XmlElement&);

    void sendMessageToInstances (const juce::MidiMessage&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputDevice)
};

}} // namespace tracktion { inline namespace engine
