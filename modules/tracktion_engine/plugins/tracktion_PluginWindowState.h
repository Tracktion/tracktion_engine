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

    void showWindowExplicitly();
    void closeWindowExplicitly();

    bool isWindowShowing() const;
    void recreateWindowIfShowing();
    void hideWindowForShutdown();

    // Attempts to use either the last saved bounds from this state,
    // or pick a default position for a newly created window that is
    // going  to hold this plugin's UI
    juce::Point<int> choosePositionForPluginWindow();

    void pluginClicked (const juce::MouseEvent&);

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
