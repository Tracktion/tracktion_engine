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

/** */
class MidiOutputDevice   : public OutputDevice
{
public:
    MidiOutputDevice (Engine&, const juce::String& name, int deviceIndex);
    ~MidiOutputDevice() override;

    void setEnabled (bool) override;
    bool isMidi() const override                        { return true; }

    virtual MidiOutputDeviceInstance* createInstance (EditPlaybackContext&);

    //==============================================================================
    juce::String prepareToPlay (Edit*, TimePosition);
    bool start();
    void stop();

    static void setControllerOffMessagesSent (Engine&, bool);
    static bool getControllerOffMessagesSent (Engine&);

    juce::String getNameForMidiNoteNumber (int note, int midiChannel, bool useSharp = true) const;

    bool isConnectedToExternalController() const;

    //==============================================================================
    void updateMidiTC (Edit*);

    void setSendingMMC (bool);
    bool isSendingMMC() const noexcept                  { return sendingMMC;    }
    bool isSendingClock() const noexcept                { return sendMidiClock; }
    void setSendingClock (bool);

    bool isSendingTimecode() const noexcept             { return sendTimecode; }
    void flipSendingTimecode();

    void setSendControllerMidiClock (bool b) noexcept   { sendControllerMidiClock = b; }
    bool isSendingControllerMidiClock() const noexcept  { return sendControllerMidiClock; }

    //==============================================================================
    void fireMessage (const juce::MidiMessage&);
    void sendNoteOffMessages();

    TimeDuration getDeviceDelay() const noexcept;

    int getPreDelayMs() const noexcept                  { return preDelayMillisecs; }
    void setPreDelayMs (int);

    //==============================================================================
    juce::StringArray getProgramSets() const;
    int getCurrentSetIndex() const;
    void setCurrentProgramSet (const juce::String&);
    juce::String getCurrentProgramSet() const           { return programNameSet; }
    juce::String getProgramName (int programNumber, int bank);
    bool canEditProgramSet (int index) const;
    bool canDeleteProgramSet (int index) const;

    juce::String getBankName (int bank);
    int getBankID (int bank);
    bool areMidiPatchesZeroBased();

    MidiProgramManager& getMidiProgramManager() const   { return engine.getMidiProgramManager(); }

protected:
    //==============================================================================
    friend class MidiOutputDeviceInstance;

    virtual void sendMessageNow (const juce::MidiMessage& message);

    void loadProps();
    void saveProps();

    int deviceIndex = 0;
    int preDelayMillisecs = 0, audioAdjustmentDelay = 0;
    std::unique_ptr<MidiTimecodeGenerator> timecodeGenerator;
    std::unique_ptr<MidiClockGenerator> midiClockGenerator;
    bool sendTimecode = false, sendMidiClock = false;
    bool playing = false;
    bool firstSetEnabledCall = true;
    juce::String programNameSet;

    double sampleRate = 0;

    std::unique_ptr<juce::MidiOutput> outputDevice;
    bool sendingMMC = false;
    bool sendControllerMidiClock = false;
    bool defaultMidiDevice = false;
    bool softDevice = false;

    juce::BigInteger midiNotesOn, channelsUsed;
    int sustain = 0;

    MidiMessageArray midiMessages;

    juce::CriticalSection noteOnLock;

    juce::String openDevice() override;
    void closeDevice() override;
};

//==============================================================================
/** Create a software midi port on macOS. Not supported on other platforms */
class SoftwareMidiOutputDevice  : public MidiOutputDevice
{
public:
    SoftwareMidiOutputDevice (Engine& e, const juce::String& deviceName)
        : MidiOutputDevice (e, deviceName, -1)
    {
        softDevice = true;
    }
};

//==============================================================================
/** */
class MidiOutputDeviceInstance    : public OutputDeviceInstance
{
public:
    MidiOutputDeviceInstance (MidiOutputDevice&, EditPlaybackContext&);
    ~MidiOutputDeviceInstance();

    juce::String prepareToPlay (TimePosition start, bool shouldSendMidiTC);
    bool start();
    void stop();

    MidiOutputDevice& getMidiOutput() const noexcept     { return static_cast<MidiOutputDevice&> (owner); }

    void mergeInMidiMessages (const MidiMessageArray&, TimePosition editTime);
    void addMidiClockMessagesToCurrentBlock (bool isPlaying, bool isDragging, TimeRange streamTime);
    MidiMessageArray& getPendingMessages() { return midiMessages; }

    // For MidiOutputDevices that aren't connected to a physical piece of hardware,
    // they should handle sending midi messages to their logical device now
    // and clear the input buffer
    virtual bool sendMessages (MidiMessageArray&, TimePosition /*editTime*/) { return false; }

private:
    std::unique_ptr<MidiTimecodeGenerator> timecodeGenerator;
    std::unique_ptr<MidiClockGenerator> midiClockGenerator;

    double sampleRate = 0, audioAdjustmentDelay = 0;
    bool playing = false, isDefaultMidiDevice = false, shouldSendMidiTimecode = false;

    MidiMessageArray midiMessages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiOutputDeviceInstance)
};

}} // namespace tracktion { inline namespace engine
