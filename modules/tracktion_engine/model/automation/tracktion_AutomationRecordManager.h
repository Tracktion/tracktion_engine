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
    static TimeDuration getGlideSeconds (Engine&);
    static void setGlideSeconds (Engine&, TimeDuration);

    Engine& engine;

private:
    //==============================================================================
    AutomationRecordManager (const AutomationRecordManager&);

    struct AutomationParamData
    {
        AutomationParamData (AutomatableParameter& p, float value)
            : parameter (p), originalValue (value)
        {
        }

        struct Change
        {
            inline Change (TimePosition t, float v) noexcept
                : time (t), value (v) {}

            TimePosition time;
            float value;
        };

        AutomatableParameter& parameter;
        juce::Array<Change> changes;
        float originalValue;
    };

    Edit& edit;
    juce::CriticalSection lock;
    juce::OwnedArray<AutomationParamData> recordedParams;
    bool writingAutomation = false;
    juce::CachedValue<bool> readingAutomation;
    bool wasPlaying = false;

    friend class AutomatableParameter;
    void postFirstAutomationChange (AutomatableParameter&, float originalValue);
    void postAutomationChange (AutomatableParameter&, TimePosition time, float value);
    void parameterBeingDeleted (AutomatableParameter&);

    void applyChangesToParameter (AutomationParamData*, TimePosition endTime, bool toEnd);

    void changeListenerCallback (juce::ChangeBroadcaster*);
};

}} // namespace tracktion { inline namespace engine
