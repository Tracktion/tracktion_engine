/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Stores automation data as it's being read in, and writes it back to the
    edit when recording finishes.

    Also looks after some global automation settings.
*/
class AutomationRecordManager   : public juce::ChangeBroadcaster,
                                  private juce::ChangeListener
{
public:
    //==============================================================================
    AutomationRecordManager (Edit&);
    ~AutomationRecordManager();

    //==============================================================================
    /** Toggles automation playback
        Matches the auto play button on the transport controls.
    */
    bool isReadingAutomation() const noexcept    { return readingAutomation; }
    void setReadingAutomation (bool);

    /** Toggles automation recording
        Matches the auto rec button on the transport controls.
    */
    bool isWritingAutomation() const noexcept    { return writingAutomation; }
    void setWritingAutomation (bool);

    /** flips the write mode, first punching out if it needs to. */
    void toggleWriteAutomationMode();

    //==============================================================================
    // Called by the transportcontrol
    void playStartedOrStopped();

    // True if some parameter data has been recorded since play began.
    bool isRecordingAutomation() const;

    // True if some param data has been recorded for this parameter since play began.
    bool isParameterRecording (AutomatableParameter*) const;

    // if we're recording, stop and write the data back to the edit.
    void punchOut (bool toEnd);

    //==============================================================================
    /** Set the 'glide' or crossfade length it'll use to patch the data back into
        the edit.
    */
    double getGlideSeconds() const;
    void setGlideSeconds (double secs);

    Engine& engine;

private:
    //==============================================================================
    AutomationRecordManager (const AutomationRecordManager&);

    struct AutomationParamData
    {
        AutomatableParameter* parameter;

        struct Change
        {
            inline Change (double t, float v) noexcept : time (t), value (v) {}

            double time;
            float value;
        };

        juce::Array<Change> changes;
        float originalValue;
    };

    Edit& edit;
    juce::CriticalSection lock;
    juce::OwnedArray<AutomationParamData> recordedParams;
    bool writingAutomation = false;
    juce::CachedValue<bool> readingAutomation;
    bool wasPlaying = false;
    double glideLength = 0;

    friend class AutomatableParameter;
    void postFirstAutomationChange (AutomatableParameter*, float originalValue);
    void postAutomationChange (AutomatableParameter*, double time, float value);
    void parameterBeingDeleted (AutomatableParameter*);

    void applyChangesToParameter (AutomationParamData*, double endTime, bool toEnd);

    void changeListenerCallback (juce::ChangeBroadcaster*);
};

} // namespace tracktion_engine
