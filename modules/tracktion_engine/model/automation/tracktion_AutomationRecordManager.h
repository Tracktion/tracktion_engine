/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

/** @internal */
class AutomationRecordManagerBase;

//==============================================================================
//==============================================================================
/** Stores automation data as it's being read in, and writes it back to the
    edit when recording finishes.

    Also looks after some global automation settings.
*/
class AutomationRecordManager
{
public:
    //==============================================================================
    AutomationRecordManager (Edit&);
    ~AutomationRecordManager();

    //==============================================================================
    /** Toggles automation playback
        Matches the auto play button on the transport controls.
    */
    bool isReadingAutomation() const noexcept;
    void setReadingAutomation (bool);

    /** Toggles automation recording
        Matches the auto rec button on the transport controls.
    */
    bool isWritingAutomation() const noexcept;
    void setWritingAutomation (bool);

    /** flips the write mode, first punching out if it needs to. */
    void toggleWriteAutomationMode();

    //==============================================================================
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
    AutomationRecordManager (const AutomationRecordManager&) = delete;

    std::unique_ptr<AutomationRecordManagerBase> pimpl;

    friend class AutomatableParameter;
    void postFirstAutomationChange (AutomatableParameter&, float originalValue);
    void postAutomationChange (AutomatableParameter&, TimePosition time, float value);
    void punchOut (AutomatableParameter&, bool toEnd);
    void parameterBeingDeleted (AutomatableParameter&);
};

} // namespace tracktion::inline engine
