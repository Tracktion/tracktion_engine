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
    Contains the limits of the various elements that can be added to an Edit.
    @see EngineBehaviour::getEditLimits
*/
struct EditLimits
{
    int maxNumTracks = 400;        /**< The maximum number of Track[s] an Edit can contain. */
    int maxClipsInTrack = 1500;    /**< The maximum number of Clip[s] a Track can contain. */
    int maxPluginsOnClip = 5;      /**< The maximum number of Plugin[s] a Clip can contain. */
    int maxPluginsOnTrack = 16;    /**< The maximum number of Plugin[s] a Track can contain. */
    int maxNumMasterPlugins = 4;   /**< The maximum number of master Plugin[s] and Edit can contain. */
};

/**
    Provides custom handlers to control various aspects of the engine's behaviour.
    Create a subclass of EngineBehaviour to customise how the engine operates
*/
class EngineBehaviour
{
public:
    EngineBehaviour() = default;
    virtual ~EngineBehaviour() = default;

    //==============================================================================
    virtual juce::ReferenceCountedObjectPtr<RackType> createPresetRackType (int /*index*/, Edit&)     { return {}; }

    /** This will be called if the PluginManager doesn't know how to create a Plugin for the given info. */
    virtual Plugin::Ptr createCustomPlugin (PluginCreationInfo)                { return {}; }

    /** Gives an opportunity to load custom plugins for those that have been registered as custom formats but not added to the list.  */
    virtual std::unique_ptr<juce::PluginDescription> findDescriptionForFileOrID (const juce::String&) { return {}; }

    /** Should return if the given plugin is disabled or not.
        ExternalPlugins will use this to determine if they should load themselves or not.
        This can be called often so should be quick to execute.
    */
    virtual bool isPluginDisabled (const juce::String& /*idString*/)                { return false; }

    /** Should implement a way of saving if plugin is disabled or not.
        N.B. only in use for ExternalPlugins at the moment.
    */
    virtual void setPluginDisabled (const juce::String& /*idString*/, bool /*shouldBeDisabled*/) {}

    /** Should the plugin be loaded. Normally plugins aren't loaded when Edit is for exporting
        or examining. Override this if you always need a plugin loaded
    */
    virtual bool shouldLoadPlugin (ExternalPlugin& p);

    /** Gives the host a chance to do any extra configuration after a plugin is loaded */
    virtual void doAdditionalInitialisation (ExternalPlugin&)                       {}

    /** If you have any special VST plugins that access items in the Edit, you need to return them */
    virtual juce::Array<Exportable::ReferencedItem> getReferencedItems (ExternalPlugin&) { return {}; }

    /** If you have any special VST plugins that access items in the Edit, you need to reassign them */
    virtual void reassignReferencedItem (ExternalPlugin&, const Exportable::ReferencedItem&, ProjectItemID, double)  {}

    /** Should return if plugins which have been bypassed should be included in the playback graph.
        By default this is false and bypassed plugins will still call processBypassed and introduce
        the same latency as if they weren't.
        But by returning false here, you can opt to remove them from the playback graph entirely
        which means they won't introduce latency which can be useful for tracking.
    */
    virtual bool shouldBypassedPluginsBeRemovedFromPlaybackGraph()                { return false; }

    /** Gives plugins an opportunity to save custom data when the plugin state gets flushed. */
    virtual void saveCustomPluginProperties (juce::ValueTree&, juce::AudioPluginInstance&, juce::UndoManager*) {}

    /** Return true if your application supports scanning plugins out of process.

        If you want to support scanning out of process, the allowing should be added
        to you JUCEApplication::initialise() function:

        void initialise (const juce::String& commandLine) override
        {
            if (PluginManager::startChildProcessPluginScan (commandLine))
                return;

             // continue like normal

      */
    virtual bool canScanPluginsOutOfProcess()                                       { return false; }

    /** You may want to disable auto initialisation of the device manager if you
        are using the engine in a plugin.
    */
    virtual bool autoInitialiseDeviceManager()                                      { return true; }

    /** In plugin builds, you might want to avoid adding the system audio devices
        and only use the host inputs.
    */
    virtual bool addSystemAudioIODeviceTypes()                                      { return true; }

    // some debate surrounds whether middle-C is C3, C4 or C5. In Tracktion we
    // default this value to 4
    virtual int getMiddleCOctave()                                                  { return 4; }
    virtual void setMiddleCOctave (int /*newOctave*/)                               {}

    // Default colour for notes. How this index maps to an actual colour is host dependant.
    // Waveform uses yellow, green, blue, purple, red, auto (based on key)
    virtual int getDefaultNoteColour()                                              { return 0; }

    // Notifies the host application that an edit has just been saved
    virtual void editHasBeenSaved (Edit& /*edit*/, juce::File /*path*/)             {}

    //==============================================================================
    /** Should return true if the incoming timestamp for MIDI messages should be used.
        If this returns false, the current system time will be used (which could be less accurate).
        N.B. this is called from multiple threads, including the MIDI thread for every
        incoming message so should be thread safe and quick to return.
    */
    virtual bool isMidiDriverUsedForIncommingMessageTiming()                        { return true; }
    virtual void setMidiDriverUsedForIncommingMessageTiming (bool)                  {}

    virtual bool shouldPlayMidiGuideNotes()                                         { return false; }

    virtual int getNumberOfCPUsToUseForAudio()                                      { return juce::jmax (1, juce::SystemStats::getNumCpus()); }

    /** Should muted tracks processing be disabled to save CPU */
    virtual bool shouldProcessMutedTracks()                                         { return false; }

    /** Should audio inputs be audible when monitor-enabled but not record enabled.  */
    virtual bool monitorAudioInputsWithoutRecordEnable()                            { return false; }

    virtual bool areAudioClipsRemappedWhenTempoChanges()                            { return true; }
    virtual void setAudioClipsRemappedWhenTempoChanges (bool)                       {}
    virtual bool areAutoTempoClipsRemappedWhenTempoChanges()                        { return true; }
    virtual void setAutoTempoClipsRemappedWhenTempoChanges (bool)                   {}
    virtual bool areMidiClipsRemappedWhenTempoChanges()                             { return true; }
    virtual void setMidiClipsRemappedWhenTempoChanges (bool)                        {}
    virtual bool arePluginsRemappedWhenTempoChanges()                               { return true; }
    virtual void setPluginsRemappedWhenTempoChanges (bool)                          {}

    /** Should return the maximum number of elements that can be added to an Edit. */
    virtual EditLimits getEditLimits()                                              { return {}; }

    /** If this returns true, it means that the length (in seconds) of one "beat" at
        any point in an edit is considered to be the length of one division in the current
        bar's time signature.
        So for example at 120BPM, in a bar of 4/4, one beat would be the length of a
        quarter-note (0.5s), but in a bar of 4/8, one beat would be the length of an
        eighth-note (0.25s)

        If false, then the length of one beat always depends only the current BPM at that
        point in the edit, so where the BPM = 120, one beat is always 0.5s, regardless of
        the time-sig.

        You shouldn't dynamically change this function's return value - just implement a
        function that always returns true or false.
    */
    virtual bool lengthOfOneBeatDependsOnTimeSignature()                            { return true; }

    struct LevelMeterSettings
    {
        int maxPeakAgeMs = 2000;
        float decaySpeed = 48.0f;
    };

    virtual LevelMeterSettings getLevelMeterSettings()                              { return {}; }
    virtual void setLevelMeterSettings (LevelMeterSettings)                         {}

    // 0 = normal, 1 = high, 2 = realtime
    virtual void setProcessPriority (int /*level*/)                                 {}

    /** If this returns true, you must implement describeWaveDevices to determine the wave devices for a given device.
        If it's false, a standard, stereo pair layout will be automatically generated.
    */
    virtual bool isDescriptionOfWaveDevicesSupported()                              { return false; }

    /** If isDescriptionOfWaveDevicesSupported returns true, this should be implemented to describe the wave devices
        for a given audio device.
    */
    virtual void describeWaveDevices (std::vector<WaveDeviceDescription>&, juce::AudioIODevice&, bool /*isInput*/) {}

    //==============================================================================
    /** Called by the MidiList to create a MidiMessageSequence for playback.
        You can override this to add your own messages but should generally follow the
        procedure in MidiList::createDefaultPlaybackMidiSequence.
    */
    virtual juce::MidiMessageSequence createPlaybackMidiSequence (const MidiList& list, MidiClip& clip, MidiList::TimeBase tb, bool generateMPE)
    {
        return MidiList::createDefaultPlaybackMidiSequence (list, clip, tb, generateMPE);
    }
    
    /** Must return the default looped sequence type to use.

        Current options are:
        0: loopRangeDefinesAllRepetitions           // The looped sequence is the same for all repetitions including the first.
        1: loopRangeDefinesSubsequentRepetitions    // The first section is the whole sequence, subsequent repitions are determined by the loop range.
    */
    virtual int getDefaultLoopedSequenceType()                                      { return 0; }

    /** If this returns true, it means that newly inserted clips will automatically have a fade-in and fade-out of 3ms applied. */
    virtual bool autoAddClipEdgeFades()                                             { return false; }

    /** Determines the default properties of clips. */
    struct ClipDefaults
    {
        bool useProxyFile = true;                                           ///< @see AudioClipBase::setUsesProxy
        ResamplingQuality resamplingQuality = ResamplingQuality::lagrange;  ///< @see setResamplingQuality::setResamplingQuality
    };

    /** Returns the defaults to be applied to new clips. */
    virtual ClipDefaults getClipDefaults()                                          { return {}; }

    struct ControlSurfaces
    {
        bool mackieMCU = true;
        bool mackieC4 = true;
        bool iconProG2 = true;
        bool tranzport = true;
        bool alphaTrack = true;
        bool remoteSL = true;
        bool remoteSLCompact = true;
        bool automap = true;
    };
    
    /** Return the control surfaces you want enabled in the engine */
    
    virtual ControlSurfaces getDesiredControlSurfaces()                             { return {}; }
};

}} // namespace tracktion { inline namespace engine
