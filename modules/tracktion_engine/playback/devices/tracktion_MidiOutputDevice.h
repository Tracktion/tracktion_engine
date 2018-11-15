/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class MidiOutputDevice   : public OutputDevice
{
public:
    MidiOutputDevice (Engine&, const juce::String& name, int deviceIndex);
    ~MidiOutputDevice();

    void setEnabled (bool) override;
    bool isMidi() const override                        { return true; }

    MidiOutputDeviceInstance* createInstance (EditPlaybackContext&);

    //==============================================================================
    juce::String prepareToPlay (Edit*, double start);
    bool start();
    void stop();

    static void setControllerOffMessagesSent (Engine&, bool);
    static bool getControllerOffMessagesSent (Engine&);

    juce::String getNameForMidiNoteNumber (int note, int midiChannel) const;

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

    //==============================================================================
    void fireMessage (const juce::MidiMessage&);
    void sendNoteOffMessages();

    double getDeviceDelay() const noexcept;

    int getPreDelayMs() const noexcept                  { return preDelayMillisecs; }
    void setPreDelayMs (int);

    //==============================================================================
    juce::StringArray getProgramSets() const;
    int getCurrentSetIndex() const;
    void setCurrentProgramSet (const juce::String&);
    juce::String getCurrentProgramSet() const           { return programNameSet; };
    juce::String getProgramName (int programNumber, int bank);
    bool canEditProgramSet (int index) const;
    bool canDeleteProgramSet (int index) const;

    juce::String getBankName (int bank);
    int getBankID (int bank);
    bool areMidiPatchesZeroBased();

    MidiProgramManager& getMidiProgramManager() const   { return engine.getMidiProgramManager(); }

private:
    //==============================================================================
    friend class MidiOutputDeviceInstance;

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

    juce::BigInteger midiNotesOn, channelsUsed;
    int sustain = 0;

    MidiMessageArray midiMessages;

    juce::CriticalSection noteOnLock;

    juce::String openDevice() override;
    void closeDevice() override;
};

//==============================================================================
/** */
class MidiOutputDeviceInstance    : public OutputDeviceInstance
{
public:
    MidiOutputDeviceInstance (MidiOutputDevice&, EditPlaybackContext&);
    ~MidiOutputDeviceInstance();

    juce::String prepareToPlay (double start, bool shouldSendMidiTC);
    bool start();
    void stop();

    MidiOutputDevice& getMidiOutput() const noexcept     { return static_cast<MidiOutputDevice&> (owner); }

    MidiMessageArray& refillBuffer (PlayHead&, EditTimeRange streamTime, int blockSize);

private:
    std::unique_ptr<MidiTimecodeGenerator> timecodeGenerator;
    std::unique_ptr<MidiClockGenerator> midiClockGenerator;

    double sampleRate = 0, audioAdjustmentDelay = 0;
    bool playing = false, isDefaultMidiDevice = false, shouldSendMidiTimecode = false;

    MidiMessageArray midiMessages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiOutputDeviceInstance)
};

} // namespace tracktion_engine
