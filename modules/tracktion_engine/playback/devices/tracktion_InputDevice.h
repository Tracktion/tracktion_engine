/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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
    ~InputDevice() override;

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
    std::atomic<bool> enabled { false };
    bool endToEndEnabled = false;
    bool retrospectiveRecordLock = false;

private:
    juce::String type, name, alias, defaultAlias;
};

//==============================================================================
class InputDeviceInstance   : protected juce::ValueTree::Listener
{
public:
    InputDeviceInstance (InputDevice&, EditPlaybackContext&);
    ~InputDeviceInstance() override;

    InputDevice& getInputDevice() noexcept      { return owner; }

    juce::Array<AudioTrack*> getTargetTracks() const;
    juce::Array<int> getTargetIndexes() const;
    
    bool isOnTargetTrack (const Track&);
    bool isOnTargetTrack (const Track&, int idx);
    
    void setTargetTrack (AudioTrack&, int index, bool moveToTrack);
    void removeTargetTrack (AudioTrack&);
    void removeTargetTrack (AudioTrack&, int index);
    void clearFromTracks();
    bool isAttachedToTrack() const;

    virtual bool isLivePlayEnabled (const Track& t) const;

    // true if recording is enabled and the input's connected to a track
    virtual bool isRecordingActive() const;
    virtual bool isRecordingActive (const Track&) const;

    bool isRecordingEnabled (const Track&) const;
    void setRecordingEnabled (const Track&, bool b);

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

    virtual juce::Array<Clip*> applyRetrospectiveRecord (SelectionManager*) = 0;

    /** can return a new node which can be used to suck data from this device
        for end-to-end'ing.
    */
    virtual AudioNode* createLiveInputNode() = 0;

    juce::ValueTree state;
    InputDevice& owner;
    EditPlaybackContext& context;
    Edit& edit;

    struct InputDeviceDestination : public Selectable
    {
        InputDeviceDestination (InputDeviceInstance& i, juce::ValueTree v)
            : input (i), state (v)
        {
            recordEnabled.referTo (state, IDs::armed, nullptr, false);
            targetTrack.referTo (state, IDs::targetTrack, nullptr);
            targetIndex.referTo (state, IDs::targetIndex, nullptr, -1);
        }
        
        ~InputDeviceDestination() override
        {
            notifyListenersOfDeletion();
        }
        
        juce::String getSelectableDescription() override
        {
            return input.getInputDevice().getSelectableDescription();
        }
        
        InputDeviceInstance& input;
        juce::ValueTree state;
        
        juce::CachedValue<bool> recordEnabled;
        juce::CachedValue<EditItemID> targetTrack;
        juce::CachedValue<int> targetIndex;
    };
    
    struct WaveInputDeviceDestination : public InputDeviceDestination
    {
        WaveInputDeviceDestination (InputDeviceInstance& i, juce::ValueTree v) : InputDeviceDestination (i, v) {}
        ~WaveInputDeviceDestination() override { notifyListenersOfDeletion(); }
    };
    
    struct MidiInputDeviceDestination : public InputDeviceDestination
    {
        MidiInputDeviceDestination (InputDeviceInstance& i, juce::ValueTree v) : InputDeviceDestination (i, v) {}
        ~MidiInputDeviceDestination() override { notifyListenersOfDeletion(); }
    };
    
    struct VirtualMidiInputDeviceDestination : public InputDeviceDestination
    {
        VirtualMidiInputDeviceDestination (InputDeviceInstance& i, juce::ValueTree v) : InputDeviceDestination (i, v) {}
        ~VirtualMidiInputDeviceDestination() override { notifyListenersOfDeletion(); }
    };

    //==============================================================================
    struct InputDeviceDestinationList  : public ValueTreeObjectList<InputDeviceDestination>
    {
        InputDeviceDestinationList (InputDeviceInstance& i, const juce::ValueTree& v)
            : ValueTreeObjectList<InputDeviceDestination> (v), input (i)
        {
            rebuildObjects();
        }

        ~InputDeviceDestinationList() override
        {
            freeObjects();
        }

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (IDs::INPUTDEVICEDESTINATION);
        }

        InputDeviceDestination* createNewObject (const juce::ValueTree& v) override
        {
            switch (input.getInputDevice().getDeviceType())
            {
                case InputDevice::waveDevice:           return new WaveInputDeviceDestination (input, v);
                case InputDevice::physicalMidiDevice:   return new MidiInputDeviceDestination (input, v);
                case InputDevice::virtualMidiDevice:    return new VirtualMidiInputDeviceDestination (input, v);
                case InputDevice::trackWaveDevice:
                case InputDevice::trackMidiDevice:
                default:
                    return new InputDeviceDestination (input, v);
            }
        }

        void deleteObject (InputDeviceDestination* c) override
        {
            delete c;
        }

        void newObjectAdded (InputDeviceDestination*) override {}
        void objectRemoved (InputDeviceDestination*) override {}
        void objectOrderChanged() override {}

        InputDeviceInstance& input;
    };
    
    InputDeviceDestination* getDestination (const Track& track, int index);

    InputDeviceDestinationList destTracks {*this, state};

    //==============================================================================
    /** Base class for classes that want to listen to an InputDevice and get a callback for each block of input.
        Subclasses of this can be used to output live audio/MIDI or for visualisations etc.
     */
    struct Consumer
    {
        /** Destructor. */
        virtual ~Consumer() = default;

        /** Override this to receive audio input from the device if it has any. */
        virtual void acceptInputBuffer (const juce::dsp::AudioBlock<float>&) {}

        /** Override this to receive MIDI input from the device if it has any. */
        virtual void handleIncomingMidiMessage (const juce::MidiMessage&) {}
    };

    /** Base classes should override this to add any Consumers internally. */
    virtual void addConsumer (Consumer*) = 0;

    /** Base classes should override this to remove the Consumer internally. */
    virtual void removeConsumer (Consumer*) = 0;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    juce::Array<AudioTrack*> activeTracks;

private:
    mutable AsyncCaller trackDeviceEnabler;
    bool wasLivePlayActive = false;
    void updateRecordingStatus();

    JUCE_DECLARE_WEAK_REFERENCEABLE (InputDeviceInstance)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputDeviceInstance)
};

} // namespace tracktion_engine
