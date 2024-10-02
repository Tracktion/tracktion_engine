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

//==============================================================================
//==============================================================================
/**
    Some useful utility functions for asyncronously loading Edits on background
    threads. Just call one of the loadEdit functions with the appropriate arguments.
*/
class EditLoader
{
public:
    //==============================================================================
    /**
        A handle to a loading Edit.
        This internally runs a thread that loads the Edit.

        You must keep the Handle alive in order to load the Edit. If it gets deleted
        it will cancel the load.
    */
    class Handle
    {
    public:
        /// Destructor. Deleting the handle cancels the load process.
        ~Handle();

        /// Cancels loading the Edit
        void cancel();

        /// Returns the progress of the Edit load
        float getProgress() const;

    private:
        friend EditLoader;
        std::thread loadThread;
        std::unique_ptr<ScopedThreadExitStatusEnabler> threadExitEnabler;
        Edit::LoadContext loadContext;

        Handle() = default;
    };

    //==============================================================================
    /** Loads an Edit asyncronously on a background thread.
        This returns a Handle with a LoadContext which you can use to cancel the
        operation or poll to get progressstatus messages.
    */
    static std::shared_ptr<Handle> loadEdit (Edit::Options,
                                             std::function<void (std::unique_ptr<Edit>)> editLoadedCallback);

    /** Loads an Edit asyncronously from a file on a background thread.
        This returns a Handle with a LoadContext which you can use to cancel the
        operation or poll to get progress.
    */
    static std::shared_ptr<Handle> loadEdit (Engine&,
                                             juce::File,
                                             std::function<void (std::unique_ptr<Edit>)> editLoadedCallback,
                                             Edit::EditRole role = Edit::forEditing,
                                             int numUndoLevelsToStore = Edit::getDefaultNumUndoLevels());
};

} // namespace tracktion::inline engine
