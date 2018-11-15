/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct PluginWindowConnection
{
    struct Slave;

    struct Master
    {
        Master (PluginWindowState&, Edit&);
        Master (PluginWindowState&, Edit&, juce::Component* pluginWindow, Slave* pluginSlaveConnection);
        ~Master();

        bool isVisible() const;
        void setVisible (bool);
        void toFront (bool setAsForeground);
        void hide();

        bool isShowing() const;
        bool shouldBeShowing() const;

        void setWindowLocked (bool isLocked);
        bool isWindowLocked() const;

        PluginInstanceWrapper* getWrapper() const;

        Plugin* plugin = nullptr;
        PluginWindowState& state;
        Edit& edit;

        // Slave should only be set in Rack windows or in non IPC mode
        Slave* slave = nullptr;

    private:
        juce::ScopedPointer<juce::Component> pluginWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Master)
    };

    struct Slave
    {
        Slave (juce::Component& pluginWindow);

        bool isVisible() const;
        void setVisible (bool);
        void toFront (bool setAsForeground);
        void hide();

        bool isShowing() const;
        bool shouldBeShowing() const;

        void setWindowLocked (bool isLocked);
        bool isWindowLocked() const;

        juce::Component& pluginWindow;
        Master* master = nullptr;

        // This is a void* because it will actually be a
        // SharedMemoryManager<TracktionArgs> which we can't forward declare
        void* sharedMemoryManager = nullptr;
        juce::int64 pluginID = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slave)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindowConnection)
};

//=============================================================================
struct PluginWindowState  : private juce::Timer
{
    PluginWindowState (Engine&);
    ~PluginWindowState() {}

    void incRefCount()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        ++windowShowerCount;
        startTimer (100);
    }

    void decRefCount()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        --windowShowerCount;
        startTimer (100);
    }

    void showWindowExplicitly()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        wasExplicitlyClosed = false;
        stopTimer();
        showWindow();
    }

    void closeWindowExplicitly()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (masterConnection != nullptr && masterConnection->isVisible())
        {
            wasExplicitlyClosed = true;
            deleteWindow();
            stopTimer();
        }
    }

    void hideWindowForShutdown()
    {
        masterConnection = nullptr;
        stopTimer();
    }

    void pickDefaultWindowBounds()
    {
        lastWindowBounds = { 100, 100, 600, 500 };

        if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
            lastWindowBounds.setPosition (focused->getTopLevelComponent()->getPosition()
                                            + juce::Point<int> (80, 80));
    }

    void pluginClicked (const juce::MouseEvent& e);

    Engine& engine;
    juce::ScopedPointer<PluginWindowConnection::Master> masterConnection;
    int windowShowerCount = 0;
    bool windowLocked;
    bool wasExplicitlyClosed = false;
    juce::Rectangle<int> lastWindowBounds;
    juce::Time windowOpenTime;

private:
    void timerCallback() override;

    void showWindow();

    void deleteWindow()
    {
        masterConnection = nullptr;
    }

    JUCE_DECLARE_WEAK_REFERENCEABLE (PluginWindowState)
    JUCE_DECLARE_NON_COPYABLE (PluginWindowState)
};

} // namespace tracktion_engine
