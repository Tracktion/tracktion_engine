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
    Create a subclass of UIBehaviour to custom UI elements creted by the engine
*/
class UIBehaviour
{
public:
    UIBehaviour() = default;
    virtual ~UIBehaviour() = default;

    //==============================================================================
    virtual Edit* getCurrentlyFocusedEdit()                                         { return {}; }
    virtual Edit* getLastFocusedEdit()                                              { return {}; }
    virtual juce::Array<Edit*> getAllOpenEdits()                                    { return {}; }
    virtual bool isEditVisibleOnScreen (const Edit&)                                { return true; }
    virtual bool closeAllEditsBelongingToProject (Project&)                         { return false; }
    virtual void editNamesMayHaveChanged()                                          {}

    virtual SelectionManager* getCurrentlyFocusedSelectionManager()                 { return {}; }
    virtual SelectionManager* getSelectionManagerForRack (const RackType&)          { return {}; }
    virtual bool paste (const Clipboard&);

    virtual Project::Ptr getCurrentlyFocusedProject()                               { return {}; }
    virtual void selectProjectInFocusedWindow (Project::Ptr)                        {}
    virtual void updateAllProjectItemLists()                                        {}

    virtual juce::ApplicationCommandManager* getApplicationCommandManager()         { return {}; }
    virtual void getAllCommands (juce::Array<juce::CommandID>&)                     {}
    virtual void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&)    {}
    virtual bool perform (const juce::ApplicationCommandTarget::InvocationInfo&)    { return false; }

    //==============================================================================
    /** Should show the new plugin window and creates the Plugin the user selects. */
    virtual Plugin::Ptr showMenuAndCreatePlugin (Plugin::Type, Edit&)               { return {}; }

    /** Must create a suitable PluginWindowConnection::Master for the given PluginWindowState.
        The type of state should be checked and used accordingly e.g. Plugin::WindowState
        or RackType::WindowsState
    */
    virtual PluginWindowConnection::Master* createPluginWindowConnection (PluginWindowState&) { return {}; }

    /** Called when a new track is created from some kind of user action i.e. not from an Edit load. */
    virtual void newTrackCreated (Track&) {}

    //==============================================================================
    /** Should display a dismissable alert window. N.B. this should be non-blocking. */
    virtual void showWarningAlert (const juce::String& title, const juce::String& message);

    /** Should display a dismissable alert window.
        Returns true for OK.
        N.B. this is blocking.
    */
    virtual bool showOkCancelAlertBox (const juce::String& title, const juce::String& message,
                                       const juce::String& ok = {},
                                       const juce::String& cancel = {});

    /** Should display a dismissable alert window.
        Returns 1 = yes, 2 = no, 0 = cancel
        N.B. this is blocking.
    */
    virtual int showYesNoCancelAlertBox (const juce::String& title, const juce::String& message,
                                         const juce::String& yes = {},
                                         const juce::String& no = {},
                                         const juce::String& cancel = {});

    /** Should display a temporary information message, usually in the same place. */
    virtual void showInfoMessage (const juce::String& message);

    /** Should display a temporary warning message. */
    virtual void showWarningMessage (const juce::String& message);

    /** Should show the current quantisation level for a short period of time. */
    virtual void showQuantisationLevel() {}

    /** Should run this task in the current window, with a progress bar, blocking
        until the task is done.
    */
    virtual void runTaskWithProgressBar (ThreadPoolJobWithProgress&)                { jassertfalse; }

    virtual bool getBigInputMetersMode()                                            { return false; }
    virtual void setBigInputMetersMode (bool) {}

    virtual bool shouldGenerateLiveWaveformsWhenRecording()                         { return true;  }

    virtual void showSafeRecordDialog (TransportControl&)                           {}
    virtual void hideSafeRecordDialog (TransportControl&)                           {}

    virtual void showProjectScreen()                                                {}
    virtual void showSettingsScreen()                                               {}
    virtual void showEditScreen()                                                   {}

    virtual void showHideVideo()                                                    {}
    virtual void showHideInputs()                                                   {}
    virtual void showHideOutputs()                                                  {}
    virtual void showHideAllPanes()                                                 {}
    virtual void toggleScroll()                                                     {}

    virtual void scrollTracksUp()                                                   {}
    virtual void scrollTracksDown()                                                 {}
    virtual void scrollTracksLeft()                                                 {}
    virtual void scrollTracksRight()                                                {}

    virtual void nudgeSelectedClips (TimecodeSnapType, const juce::String& commandDesc,
                                     SelectionManager&, const juce::Array<Clip*>&, bool automationLocked);
    virtual void nudgeSelected (TimecodeSnapType, const juce::String& commandDesc, bool automationLocked);
    virtual void nudgeSelected (const juce::String& commandDesc);

    virtual void stopPreviewPlayback()                                              {}
    virtual void resetOverloads()                                                   {}

    virtual void zoomHorizontal (float /*increment*/)                               {}
    virtual void zoomVertical (float /*amount*/)                                    {}
    virtual void zoomToSelection()                                                  {}
    virtual void zoomToFitHorizontally()                                            {}
    virtual void zoomToFitVertically()                                              {}

    //==============================================================================
    /** Should return the position which used be used for edit operations such as splitting.
        By default this returns the transport position.
    */
    virtual double getEditingPosition (Edit&);

    /** Should return the range which used be used for edit operations such as coping or deleting.
        By default this returns the loop range.
    */
    virtual EditTimeRange getEditingRange (Edit&);

    //==============================================================================
    /** If your UI has the concept of edit groups, you should return an expanded list of
        selected items that includes all clips that should be edited with the selected
        clip */
    virtual SelectableList getAssociatedClipsToEdit (const SelectableList& items)   { return items; }
};

} // namespace tracktion_engine
