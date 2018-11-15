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
    Asyncronously call a function.
*/
struct AsyncCaller  : public juce::AsyncUpdater
{
    /** Creates an empty AsyncFunctionCaller. */
    AsyncCaller() = default;

    /** Destructor. */
    ~AsyncCaller()
    {
        cancelPendingUpdate();
    }

    /** Sets the function to call. */
    void setFunction (std::function<void()> f)
    {
        function = std::move (f);
    }

    /** @internal. */
    void handleAsyncUpdate() override
    {
        jassert (function);
        function();
    }

    std::function<void()> function;
};

//==============================================================================
/**
    Holds a list of function objects and enables you to call them asyncronously.
*/
struct AsyncFunctionCaller  : private juce::AsyncUpdater
{
    /** Creates an empty AsyncFunctionCaller. */
    AsyncFunctionCaller() {}

    /** Destructor. */
    ~AsyncFunctionCaller()
    {
        cancelPendingUpdate();
    }

    /** Adds a function and associates a functionID with it. */
    void addFunction (int functionID, const std::function<void()>& f)
    {
        functions[functionID] = { false, f };
    }

    /** Triggers an asyncronous call to one of the functions. */
    void updateAsync (int functionID)
    {
        auto found = functions.find (functionID);

        if (found != functions.end())
        {
            found->second.first = true;
            triggerAsyncUpdate();
        }
    }

    /** @internal. */
    void handleAsyncUpdate() override
    {
        auto compareAndReset = [] (bool& flag) -> bool
        {
            if (! flag)
                return false;

            flag = false;
            return true;
        };

        for (auto&& f : functions)
            if (compareAndReset (f.second.first))
                f.second.second();
    }

    std::map<int, std::pair<bool, std::function<void (void)>>> functions;
};

//==============================================================================
class LambdaTimer   : public juce::Timer
{
public:
    LambdaTimer() = default;

    LambdaTimer (std::function<void()> newCallback)
    {
        callback = std::move (newCallback);
    }

    void setCallback (std::function<void()> newCallback)
    {
        callback = std::move (newCallback);
    }

    void timerCallback() override
    {
        if (callback)
            callback();
    }

private:
    std::function<void()> callback;
};

//==============================================================================
/** Calls a function on the message thread checking a calling thread for an exit signal. */
class MessageThreadCallback   : private juce::AsyncUpdater
{
public:
    MessageThreadCallback()                 {}
    ~MessageThreadCallback()                { cancelPendingUpdate(); }

    /** Returns true if the callback has completed. */
    bool hasFinished() const noexcept       { return finished.get() != 0; }

    /** Triggers the callback to happen on the message thread and blocks
        until it has returned or the thread signals it should exit.
    */
    void triggerAndWaitForCallback()
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            handleAsyncUpdate();
            return;
        }

        triggerAsyncUpdate();

        if (auto job = juce::ThreadPoolJob::getCurrentThreadPoolJob())
        {
            while (! (job->shouldExit() || hasFinished()))
                waiter.wait (50);

            return;
        }

        if (auto thread = juce::Thread::getCurrentThread())
        {
            while (! (thread->threadShouldExit() || hasFinished()))
                waiter.wait (50);

            return;
        }

        TRACKTION_LOG_ERROR ("Rogue call to triggerAndWaitForCallback()");
        jassertfalse;
        waiter.wait (50);

        if (finished.get() == 0)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("triggerAndWaitForCallback() unable to complete");
        }
    }

    virtual void performAction() = 0;

private:
    juce::Atomic<int> finished;
    juce::WaitableEvent waiter;

    void handleAsyncUpdate() override
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        performAction();
        finished.set (1);
        waiter.signal();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageThreadCallback)
};

//==============================================================================
struct BlockingFunction  : public MessageThreadCallback
{
    BlockingFunction (std::function<void(void)> f)  : fn (f) {}

private:
    std::function<void(void)> fn;
    void performAction() override    { fn(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockingFunction)
};

inline void callBlocking (std::function<void(void)> f)
{
    BlockingFunction (f).triggerAndWaitForCallback();
}

} // namespace tracktion_engine
