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
    InputDevice (Engine&, juce::String type, juce::String name, juce::String deviceID);
    ~InputDevice() override;

    //==============================================================================
    const juce::String& getName() const         { return name; }
    const juce::String& getType() const         { return type; }

    juce::String getDeviceID() const            { return deviceID; }

    virtual DeviceType getDeviceType() const = 0;
    bool isTrackDevice() const;

    /** the alias is the name shown in the draggable input device components */
    juce::String getAlias() const;
    void setAlias (const juce::String& newAlias);

    virtual bool isAvailableToEdit() const      { return isEnabled(); }

    bool isEnabled() const;
    virtual void setEnabled (bool) = 0;

    virtual bool isMidi() const                 { return false; }

    /** Enum to describe monitor modes. */
    enum class MonitorMode
    {
        off,        ///< Live input is never audible
        automatic,  ///< Live input is audible when record is enabled
        on          ///< Live input is always audible
    };

    MonitorMode getMonitorMode() const          { return monitorMode; }
    void setMonitorMode (MonitorMode);

    /** Creates an instance to use for a given playback context. */
    virtual InputDeviceInstance* createInstance (EditPlaybackContext&) = 0;

    /** This is a bit of a hack but allows the time for MIDI devices to be set through the base class interface. */
    virtual void masterTimeUpdate (double time) = 0;

    static void setRetrospectiveLock (Engine&, const juce::Array<InputDeviceInstance*>&, bool lock);
    virtual void updateRetrospectiveBufferLength (double length) = 0;

    //==============================================================================
    juce::String getSelectableDescription() override;

    virtual void saveProps() = 0;

    Engine& engine;
    LevelMeasurer levelMeasurer;

protected:
    std::atomic<bool> enabled { false };
    MonitorMode monitorMode = MonitorMode::automatic;
    MonitorMode defaultMonitorMode = MonitorMode::automatic;
    bool retrospectiveRecordLock = false;

private:
    const juce::String type, deviceID, name;
    juce::String alias;

    juce::String getAliasPropName() const;
};


//==============================================================================
//==============================================================================
/**
    An instance of an InputDevice that's available to an Edit.
    Whereas InputDevice[s] are Engine level objects, instances apply to an Edit
    and can therefore be assigned to slots/tracks etc.
    @see EditInputDevices
*/
class InputDeviceInstance   : protected juce::ValueTree::Listener
{
public:
    struct Destination;

    //==============================================================================
    //==============================================================================
    /** @internal */
    InputDeviceInstance (InputDevice&, EditPlaybackContext&);

    /** Destructor. */
    ~InputDeviceInstance() override;

    /** Returns the InputDevice this instance belongs to. */
    InputDevice& getInputDevice() noexcept      { return owner; }

    /** Returns the targets this instance is assigned to. */
    juce::Array<EditItemID> getTargets() const;

    /** Assigns this input to either an AudioTrack or a ClipSlot. */
    [[ nodiscard ]] tl::expected<Destination*, juce::String> setTarget (EditItemID targetID, bool moveToTrack, juce::UndoManager*,
                                                                        std::optional<int> index = std::nullopt);

    /** Removes the destination with the given targetID. */
    [[ nodiscard ]] juce::Result removeTarget (EditItemID targetID, juce::UndoManager*);

    /** Whether the track should monitor the input or not. */
    virtual bool isLivePlayEnabled (const Track&) const;

    /** Returns true if recording is enabled and the input is connected to any target. */
    virtual bool isRecordingActive() const;

    /** Returns true if recording is enabled and the input is connected the given target. */
    virtual bool isRecordingActive (EditItemID) const;

    /** Returns true if the async stopRecording function has been used and this target is waiting to stop. */
    virtual bool isRecordingQueuedToStop (EditItemID) = 0;

    /** Returns true if recording is enabled for the given target. */
    bool isRecordingEnabled (EditItemID) const;

    /** Enabled/disables recording for a given target.
        If the transport is currently recording, this will start a punch-in recording.
    */
    void setRecordingEnabled (EditItemID, bool);

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

    /** Stops a recording from receiving any more input.
        Because stopRecording is blocking, if you have many inputs recording, some inputs can continue to
        record whilst others are having their files closed. Calling this first prevents this.
        @param targetsToStop    The targets to stop, others will continue allowing you to punch out
                                only specific targets. If this is empty, all active recordings will be stopped.
    */
    virtual void prepareToStopRecording (std::vector<EditItemID> targetsToStop) = 0;

    /** Stops a recording.
        @param StopRecordingParameters determines how stopped recordings are treated.
    */
    virtual tl::expected<Clip::Array, juce::String> stopRecording (StopRecordingParameters) = 0;

    /** Stops a recording asyncronously.
        This can be used to trigger a recording to stop at a time in the future.
        This function will return immidiately and call the provided callback when the recording actually stops.
        @param StopRecordingParameters      Determines how stopped recordings are treated.
        @param recordingStoppedCallback     A callback to call when the recording stops.
    */
    virtual void stopRecording (StopRecordingParameters,
                                std::function<void (tl::expected<Clip::Array, juce::String>)>) = 0;

    /** Returns true if there are any active recordings for this device. */
    virtual bool isRecording (EditItemID) = 0;

    /** Returns true if there are any active recordings for this device. */
    virtual bool isRecording() = 0;

    /** Returns the File that the given target is currently recording to. */
    virtual juce::File getRecordingFile (EditItemID) const     { return {}; }

    /** Returns a fifo of recorded MIDInotes that can be used for drawing UI components */
    virtual std::shared_ptr<choc::fifo::SingleReaderSingleWriterFIFO<juce::MidiMessage>> getRecordingNotes (EditItemID) const { return nullptr; }

    /** Returns the time that a given target started recording. */
    virtual TimePosition getPunchInTime (EditItemID) = 0;

    /** Takes the retrospective buffer and creates clips from it, as if recording had been
        triggered in the past and stopped at the time of calling this function.
    */
    virtual juce::Array<Clip*> applyRetrospectiveRecord (bool armedOnly) = 0;

    //==============================================================================
    juce::ValueTree state;          /** The state of this instance. */
    InputDevice& owner;             /**< The InputDevice this is an instance of. */
    EditPlaybackContext& context;   /**< The EditPlaybackContext this instance belongs to. */
    Edit& edit;                     /**< The Edit this instance belongs to. */

    //==============================================================================
    struct Destination : public Selectable
    {
        /** Destructor. */
        ~Destination() override
        {
            notifyListenersOfDeletion();
        }

        InputDeviceInstance& input; /**< The instance this belongs to. */
        juce::ValueTree state;      /**< The state of this destination. */

        /** The target for this destination, either an AudioTrack or ClipSlot. */
        const EditItemID targetID = EditItemID::fromProperty (state, IDs::targetID);

        /** The target index for this destination, if it is an AudioTrack. */
        const int targetIndex = state[IDs::targetIndex];

        /** Property to control whether the destination is armed to record or not. */
        juce::CachedValue<bool> recordEnabled;

        //==============================================================================
        /** @internal */
        Destination (InputDeviceInstance& i, juce::ValueTree v)
            : input (i), state (v)
        {
            recordEnabled.referTo (state, IDs::armed, nullptr, false);
            assert (targetID.isValid() && "Must have a valid ID!");
        }

        /** @internal */
        juce::String getSelectableDescription() override
        {
            return input.getInputDevice().getSelectableDescription();
        }
    };

    //==============================================================================
    struct WaveInputDestination : public Destination
    {
        WaveInputDestination (InputDeviceInstance& i, juce::ValueTree v) : Destination (i, v) {}
        ~WaveInputDestination() override { notifyListenersOfDeletion(); }
    };

    struct MidiInputDestination : public Destination
    {
        MidiInputDestination (InputDeviceInstance& i, juce::ValueTree v) : Destination (i, v) {}
        ~MidiInputDestination() override { notifyListenersOfDeletion(); }
    };

    struct VirtualMidiInputDestination : public Destination
    {
        VirtualMidiInputDestination (InputDeviceInstance& i, juce::ValueTree v) : Destination (i, v) {}
        ~VirtualMidiInputDestination() override { notifyListenersOfDeletion(); }
    };

    //==============================================================================
    struct DestinationList  : public ValueTreeObjectList<Destination>
    {
        DestinationList (InputDeviceInstance& i, const juce::ValueTree& v)
            : ValueTreeObjectList<Destination> (v), input (i)
        {
            rebuildObjects();
        }

        ~DestinationList() override
        {
            freeObjects();
        }

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (IDs::INPUTDEVICEDESTINATION);
        }

        Destination* createNewObject (const juce::ValueTree& v) override
        {
            switch (input.getInputDevice().getDeviceType())
            {
                case InputDevice::waveDevice:           [[ fallthrough ]];
                case InputDevice::trackWaveDevice:      return new WaveInputDestination (input, v);
                case InputDevice::physicalMidiDevice:   [[ fallthrough ]];
                case InputDevice::trackMidiDevice:      return new MidiInputDestination (input, v);
                case InputDevice::virtualMidiDevice:    return new VirtualMidiInputDestination (input, v);
                default:
                {
                    jassertfalse;
                    return new Destination (input, v);
                }
            }
        }

        void deleteObject (Destination* c) override
        {
            delete c;
        }

        void newObjectAdded (Destination*) override {}
        void objectRemoved (Destination*) override {}
        void objectOrderChanged() override {}

        InputDeviceInstance& input;
    };

    /** The list of assigned destinations. */
    DestinationList destinations { *this, state };

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
        virtual void handleIncomingMidiMessage (const juce::MidiMessage&, MPESourceID) {}

        /** This is called when a recording is aborted so listeners should trash their temp data */
        virtual void discardRecordings (EditItemID /*targetID*/) {}
    };

    /** Base classes should override this to add any Consumers internally. */
    virtual void addConsumer (Consumer*) = 0;

    /** Base classes should override this to remove the Consumer internally. */
    virtual void removeConsumer (Consumer*) = 0;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    ClipSlot* getFreeSlot (AudioTrack&);

private:
    mutable AsyncCaller trackDeviceEnabler, recordStatusUpdater;
    bool wasLivePlayActive = false, destinationsChanged = false;
    std::vector<EditItemID> changedTargetTrackIDs;
    void updateRecordingStatus();
    void handleDestinationChange (EditItemID, bool);

    JUCE_DECLARE_WEAK_REFERENCEABLE (InputDeviceInstance)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputDeviceInstance)
};


//==============================================================================
//==============================================================================
/** Returns the AudioTracks and their indexes this instance is assigned to. */
[[ nodiscard ]] juce::Array<std::pair<AudioTrack*, int>> getTargetTracksAndIndexes (InputDeviceInstance&);

/** Returns the AudioTracks this instance is assigned to. */
[[ nodiscard ]] juce::Array<AudioTrack*> getTargetTracks (InputDeviceInstance&);

/** Returns true if this instance is assigned to the given Track at the given index . */
[[ nodiscard ]] bool isOnTargetTrack (InputDeviceInstance&, const Track&, int idx);

/** Returns true if this input is assigned to a target. */
[[ nodiscard ]] bool isAttached (InputDeviceInstance&);

/** Removes this instance from all assigned targets. */
[[ nodiscard ]] juce::Result clearFromTargets (InputDeviceInstance&, juce::UndoManager*);

//==============================================================================
/** Returns the destination if one has been assigned for the given arguments. */
[[ nodiscard ]] InputDeviceInstance::Destination* getDestination (InputDeviceInstance&, const Track& track, int index);

/** Returns the destination if one has been assigned for the given arguments. */
[[ nodiscard ]] InputDeviceInstance::Destination* getDestination (InputDeviceInstance&, const ClipSlot&);

/** Returns the destination if one has been assigned for the given arguments. */
[[ nodiscard ]] InputDeviceInstance::Destination* getDestination (InputDeviceInstance&, const juce::ValueTree& destinationState);


//==============================================================================
/** Returns true if all the targets were fully prepared. */
[[ nodiscard ]] inline bool hasErrors (const InputDeviceInstance::PreparedContext& pc)
{
    for (auto& res : pc)
        if (! res)
            return true;

    return false;
}

/** Appends a PreparedContent to another. */
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
