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
    virtual const juce::PluginDescription* findDescriptionForFileOrID (const juce::String&)     { return {}; }

    /** Should return if the given plugin is disabled or not.
        ExternalPlugins will use this to determine if they should load themselves or not.
        This can be called often so should be quick to execute.
    */
    virtual bool isPluginDisabled (const juce::String& /*idString*/)                { return false; }

    /** Should implement a way of saving if plugin is disabled or not.
        N.B. only in use for ExternalPlugins at the moment.
    */
    virtual void setPluginDisabled (const juce::String& /*idString*/, bool /*shouldBeDisabled*/) {}

    /** Gives plugins an opportunity to save custom data when the plugin state gets flushed. */
    virtual void saveCustomPluginProperties (juce::ValueTree&, juce::AudioPluginInstance&, juce::UndoManager*) {}

    // some debate surrounds whether middle-C is C3, C4 or C5. In Tracktion we
    // default this value to 4
    virtual int getMiddleCOctave()                                                  { return 4; }
    virtual void setMiddleCOctave (int /*newOctave*/)                               {}

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

    virtual bool areAudioClipsRemappedWhenTempoChanges()                            { return true; }
    virtual void setAudioClipsRemappedWhenTempoChanges (bool)                       {}
    virtual bool areAutoTempoClipsRemappedWhenTempoChanges()                        { return true; }
    virtual void setAutoTempoClipsRemappedWhenTempoChanges (bool)                   {}
    virtual bool areMidiClipsRemappedWhenTempoChanges()                             { return true; }
    virtual void setMidiClipsRemappedWhenTempoChanges (bool)                        {}
    virtual bool arePluginsRemappedWhenTempoChanges()                               { return true; }
    virtual void setPluginsRemappedWhenTempoChanges (bool)                          {}

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
    virtual bool lengthOfOneBeatDependsOnTimeSignature()                            { return false; }

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

    /** Must return the default looped sequence type to use.

        Current options are:
        0: loopRangeDefinesAllRepetitions           // The looped sequence is the same for all repititions including the first.
        1: loopRangeDefinesSubsequentRepetitions    // The first section is the whole sequence, subsequent repitions are determined by the loop range.
    */
    virtual int getDefaultLoopedSequenceType()                                      { return 0; }
};

} // namespace tracktion_engine
