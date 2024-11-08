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

struct PluginWindowState  : private juce::Timer
{
    PluginWindowState (Edit&);

    void incRefCount();
    void decRefCount();

    //==============================================================================
    bool isWindowShowing() const;

    void showWindowExplicitly();
    void showWindowIfTemporarilyHidden();
    void recreateWindowIfShowing();

    void closeWindowExplicitly();
    void hideWindowForShutdown();
    void hideWindowTemporarily();

    /// Calls hideWindowTemporarily() for any windows of plugins in this edit
    static void hideAllWindowsTemporarily (Edit&);
    /// Calls showWindowIfTemporarilyHidden() for all plugins in this edit
    static void showAllTemporarilyHiddenWindows (Edit&);

    /// If any windows are showing, hide them all temporarily, otherwise
    /// bring back any temporarily hidden ones.
    static void showOrHideAllWindows (Edit&);

    /// Finds all windows for all plugins in this edit
    static std::vector<PluginWindowState*> getAllWindows (Edit&);
    /// Counts the number of visible windows for plugins in this edit
    static uint32_t getNumOpenWindows (Edit&);

    // Attempts to use either the last saved bounds from this state,
    // or pick a default position for a newly created window that is
    // going  to hold this plugin's UI
    juce::Point<int> choosePositionForPluginWindow();

    /// Can be used to manually fire a mouse event into the window
    void pluginClicked (const juce::MouseEvent&);

    //==============================================================================
    Edit& edit;
    Engine& engine;
    std::unique_ptr<juce::Component> pluginWindow;
    int windowShowerCount = 0;
    bool windowLocked;
    bool temporarilyHidden = false;
    bool wasExplicitlyClosed = false;
    std::optional<juce::Rectangle<int>> lastWindowBounds;
    juce::Time windowOpenTime;

private:
    void timerCallback() override;

    void showWindow();
    void deleteWindow();

    JUCE_DECLARE_WEAK_REFERENCEABLE (PluginWindowState)
    JUCE_DECLARE_NON_COPYABLE (PluginWindowState)
};

}} // namespace tracktion { inline namespace engine
