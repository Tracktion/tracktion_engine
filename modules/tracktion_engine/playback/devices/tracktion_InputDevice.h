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

    /** Called after all devices are constructed, so it can use all the device
        names in its calculations.
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
//==============================================================================
class InputDeviceInstance   : protected juce::ValueTree::Listener
{
public:
    struct InputDeviceDestination;

    InputDeviceInstance (InputDevice&, EditPlaybackContext&);
    ~InputDeviceInstance() override;

    InputDevice& getInputDevice() noexcept      { return owner; }

    juce::Array<EditItemID> getTargets() const;

    juce::Array<AudioTrack*> getTargetTracks() const;
    juce::Array<int> getTargetIndexes() const;

    bool isOnTargetTrack (const Track&);
    bool isOnTargetTrack (const Track&, int idx);
    bool isOnTargetSlot (const ClipSlot&);

    void setTargetTrack (AudioTrack&, int index, bool moveToTrack, juce::UndoManager*);
    void removeTargetTrack (AudioTrack&, juce::UndoManager*);
    void removeTargetTrack (AudioTrack&, int index, juce::UndoManager*);
    void removeTargetTrack (EditItemID, int index, juce::UndoManager*);
    void clearFromTracks (juce::UndoManager*);
    bool isAttachedToTrack() const;

    InputDeviceDestination* setTarget (EditItemID targetID, bool moveToTrack, juce::UndoManager*);
    void removeTarget (EditItemID targetID, juce::UndoManager*);

    virtual bool isLivePlayEnabled (const Track&) const;

    // true if recording is enabled and the input's connected to a track
    virtual bool isRecordingActive() const;
    virtual bool isRecordingActive (const Track&) const;

    bool isRecordingEnabled (EditItemID) const;
    void setRecordingEnabled (const Track&, bool b);

    /** Should return true if this input is currently actively recording into a track
        and it wants the existing track contents to be muted.
    */
    virtual bool shouldTrackContentsBeMuted (const Track&)   { return false; }

    //==============================================================================
    //** The parameters used to configure a recording operation. */
    struct RecordingParameters
    {
        TimeRange punchRange;               /**< The transport time range at which the recording should happen. */
        std::vector<EditItemID> targets;    /**< The targets to record to, if this is empty, all armed targets will be added. */
    };

    /**
        Base class for RecordingContexts.
        These are essentially handles returned from prepareToRecord and
        then passed to startRecording where they are consumed.
    */
    class RecordingContext
    {
    public:
        /** Destructor. */
        virtual ~RecordingContext() = default;

        const EditItemID targetID; /**< The ID of the recording target, track or clip. */

    protected:
        RecordingContext (EditItemID targetID_)
            : targetID (targetID_)
        {}
    };

    /** An array of either valid RecordingContexts or an error message if the recording couldn't be started.
        @see
    */
    using PreparedContext = std::vector<tl::expected<std::unique_ptr<RecordingContext>, juce::String>>;

    /** Prepares a recording operation.
        @param      RecordingParameters  Determines the destinations and punch ranges
        @returns    An array of either valid RecordingContexts or error messages
    */
    [[nodiscard]] virtual PreparedContext prepareToRecord (RecordingParameters) = 0;

    /** Starts a recording.
        @param      The prepared recording contexts to start.
        @returns    Any recording contexts that couldn't be started by this device.
                    E.g. If the contexts are mixed MIDI and audio and this is an audio device,
                    it will returns the MIDI contexts to pass to a MIDI device.
    */
    virtual std::vector<std::unique_ptr<RecordingContext>> startRecording (std::vector<std::unique_ptr<RecordingContext>>) = 0;

    /**
        The params passed to stopRecording.
    */
    struct StopRecordingParameters
    {
        std::vector<EditItemID> targetsToStop;      /**< The targets to stop, others will continue allowing you to punch out
                                                         only specific targets. If this is empty, all active recordings will be stopped. */
        TimePosition unloopedTimeToEndRecording;    /**< The TimePosition this recording should be stopped at
                                                         @see EditPlaybackContext::getUnloopedPosition. */
        bool isLooping = false;                     /**< Whether to treat the stopped recordings as looped or not. */
        TimeRange markedRange;                      /**< The marked range used for either loop or punch times. */
        bool discardRecordings = false;             /**< Whether to discard recordings or keep them. */
    };

    /** Stops a recording.
        @param StopRecordingParameters determines how stopped recordings are treated.
    */
    virtual Clip::Array stopRecording (StopRecordingParameters) = 0;

    /** Returns true if there are any active recordings for this device. */
    virtual bool isRecording() = 0;

    //ddd Delete this and just use the targetID
    virtual juce::File getRecordingFile (EditItemID) const     { return {}; }

    /** Returns the time that a given target started recording. */
    virtual TimePosition getPunchInTime (EditItemID) = 0;

    /** Takes the retrospective buffer and creates clips from it, as if recording had been
        triggered in the past and stopped at the time of calling this function.
    */
    virtual juce::Array<Clip*> applyRetrospectiveRecord() = 0;

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
            assert (targetTrack.isValid() != targetSlot.isValid()
                    && "One target must be valid!");
        }

        ~InputDeviceDestination() override
        {
            notifyListenersOfDeletion();
        }

        juce::String getSelectableDescription() override
        {
            return input.getInputDevice().getSelectableDescription();
        }

        EditItemID getTarget() const
        {
            return targetTrack.isValid() ? targetTrack : targetSlot;
        }

        InputDeviceInstance& input;
        juce::ValueTree state;

        juce::CachedValue<bool> recordEnabled;

    //dddprivate:
        const EditItemID targetTrack = EditItemID::fromProperty (state, IDs::targetTrack);
        const EditItemID targetSlot = EditItemID::fromProperty (state, IDs::targetSlot);
        const int targetIndex = state[IDs::targetIndex];
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
                case InputDevice::waveDevice:           [[ fallthrough ]];
                case InputDevice::trackWaveDevice:      return new WaveInputDeviceDestination (input, v);
                case InputDevice::physicalMidiDevice:   [[ fallthrough ]];
                case InputDevice::trackMidiDevice:      return new MidiInputDeviceDestination (input, v);
                case InputDevice::virtualMidiDevice:    return new VirtualMidiInputDeviceDestination (input, v);
                default:
                {
                    jassertfalse;
                    return new InputDeviceDestination (input, v);
                }
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
    InputDeviceDestination* getDestination (const ClipSlot&);
    InputDeviceDestination* getDestination (const juce::ValueTree& destinationState);

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
        virtual void acceptInputBuffer (choc::buffer::ChannelArrayView<float>) {}

        /** Override this to receive MIDI input from the device if it has any. */
        virtual void handleIncomingMidiMessage (const juce::MidiMessage&) {}

        /** This is called when a recording is aborted so listeners should trash their temp data */
        virtual void discardRecordings() {}
    };

    /** Base classes should override this to add any Consumers internally. */
    virtual void addConsumer (Consumer*) = 0;

    /** Base classes should override this to remove the Consumer internally. */
    virtual void removeConsumer (Consumer*) = 0;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

private:
    mutable AsyncCaller trackDeviceEnabler;
    bool wasLivePlayActive = false;
    void updateRecordingStatus (EditItemID);

    JUCE_DECLARE_WEAK_REFERENCEABLE (InputDeviceInstance)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputDeviceInstance)
};


//==============================================================================
//==============================================================================
/** Returns true if all the targets were fully prepared. */
[[ nodiscard ]] inline bool hasErrors (const InputDeviceInstance::PreparedContext& pc)
{
    for (auto& res : pc)
        if (! res)
            return true;

    return false;
}

inline InputDeviceInstance::PreparedContext& append (InputDeviceInstance::PreparedContext& dest, InputDeviceInstance::PreparedContext&& src)
{
    std::move (src.begin(), src.end(), std::back_inserter(dest));
    return dest;
}

/** Splits the PreparedContext in to valid RecordingContexts and an array of error messages. */
[[ nodiscard ]] inline std::pair<std::vector<std::unique_ptr<InputDeviceInstance::RecordingContext>>, juce::StringArray> extract (InputDeviceInstance::PreparedContext&& pc)
{
    std::vector<std::unique_ptr<InputDeviceInstance::RecordingContext>> recContexts;
    juce::StringArray errors;

    for (auto& res : pc)
    {
        if (res)
            recContexts.emplace_back (std::move (res.value()));
        else
            errors.add (res.error());
    }

    return { std::move (recContexts), std::move (errors) };
}


}} // namespace tracktion { inline namespace engine
