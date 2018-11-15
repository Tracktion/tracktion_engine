/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Represents an input device.

    A single InputDevice object exists for each device in the system.
    For each InputDevice, there may be multiple InputDeviceInstance objects, for
    all the active EditPlaybackContexts
*/
class InputDevice   : public Selectable
{
public:
    /** enum to allow quick querying of the device type. */
    enum DeviceType
    {
        waveDevice,
        trackWaveDevice,
        physicalMidiDevice,
        virtualMidiDevice,
        trackMidiDevice
    };

    //==============================================================================
    InputDevice (Engine&, const juce::String& type, const juce::String& name);
    ~InputDevice();

    //==============================================================================
    const juce::String& getName() const         { return name; }
    const juce::String& getType() const         { return type; }

    virtual DeviceType getDeviceType() const = 0;
    bool isTrackDevice() const;

    /** the alias is the name shown in the draggable input device components */
    juce::String getAlias() const;
    void setAlias (const juce::String& newAlias);

    /** called after all devices are constructed, so it can use all the device
        names in its calculations..
    */
    void initialiseDefaultAlias();

    virtual bool isAvailableToEdit() const      { return isEnabled(); }

    bool isEnabled() const;
    virtual void setEnabled (bool) = 0;

    virtual bool isMidi() const                 { return false; }

    bool isEndToEndEnabled() const              { return endToEndEnabled; }
    virtual void flipEndToEnd() = 0;

    /** Creates an instance to use for a given playback context. */
    virtual InputDeviceInstance* createInstance (EditPlaybackContext&) = 0;

    /** This is a bit of a hack but allows the time for MIDI devices to be set through the base class interface. */
    virtual void masterTimeUpdate (double time) = 0;

    static void setRetrospectiveLock (Engine& engine, const juce::Array<InputDeviceInstance*>& devices, bool lock);
    virtual void updateRetrospectiveBufferLength (double length) = 0;

    //==============================================================================
    juce::String getSelectableDescription() override;

    Engine& engine;
    LevelMeasurer levelMeasurer;

    juce::String getGlobalPropertyName() const;

protected:
    bool enabled = false, endToEndEnabled = false;
    bool retrospectiveRecordLock = false;

private:
    juce::String type, name, alias, defaultAlias;
};

//==============================================================================
class InputDeviceInstance   : protected juce::ValueTree::Listener
{
public:
    InputDeviceInstance (InputDevice&, EditPlaybackContext&);
    virtual ~InputDeviceInstance();

    InputDevice& getInputDevice() noexcept      { return owner; }

    AudioTrack* getTargetTrack() const;
    int getTargetIndex() const;
    void setTargetTrack (AudioTrack*, int index);
    void clearFromTrack();
    bool isAttachedToTrack() const;

    virtual bool isLivePlayEnabled() const;

    // true if recording is enabled and the input's connected to a track
    virtual bool isRecordingActive() const;

    bool isRecordingEnabled() const             { return recordEnabled; }
    void setRecordingEnabled (bool b)           { recordEnabled = b; }

    /** should return true if this input is currently actively recording into a track
        and it wants the existing track contents to be muted.
    */
    virtual bool shouldTrackContentsBeMuted()   { return false; }

    virtual juce::String prepareToRecord (double start, double punchIn,
                                          double sampleRate, int blockSizeSamples,
                                          bool isLivePunch) = 0;

    virtual bool startRecording() = 0;
    virtual bool isRecording() = 0;
    virtual void stop() = 0;

    // called if not all devices started correctly when recording started.
    virtual void recordWasCancelled() = 0;
    virtual juce::File getRecordingFile() const     { return {}; }

    virtual void prepareAndPunchRecord();
    virtual double getPunchInTime() = 0;
    virtual Clip::Array stopRecording() = 0;
    virtual Clip::Array applyLastRecordingToEdit (EditTimeRange recordedRange,
                                                  bool isLooping, EditTimeRange loopRange,
                                                  bool discardRecordings,
                                                  SelectionManager*) = 0;

    virtual Clip* applyRetrospectiveRecord (SelectionManager*) = 0;

    /** can return a new node which can be used to suck data from this device
        for end-to-end'ing.
    */
    virtual AudioNode* createLiveInputNode() = 0;

    juce::ValueTree state;
    InputDevice& owner;
    EditPlaybackContext& context;
    Edit& edit;

    juce::CachedValue<bool> recordEnabled;
    juce::CachedValue<EditItemID> targetTrack;
    juce::CachedValue<int> targetIndex;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

private:
    mutable AsyncCaller trackDeviceEnabler;
    bool wasLivePlayActive = false;
    void updateRecordingStatus();

    JUCE_DECLARE_WEAK_REFERENCEABLE (InputDeviceInstance)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputDeviceInstance)
};

} // namespace tracktion_engine
