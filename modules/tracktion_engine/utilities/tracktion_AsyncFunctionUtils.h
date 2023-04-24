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

/**
    Asyncronously call a function.
*/
struct AsyncCaller  : public juce::AsyncUpdater
{
    /** Creates an empty AsyncFunctionCaller. */
    AsyncCaller() = default;

    /** Destructor. */
    ~AsyncCaller() override
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
    AsyncFunctionCaller() = default;

    /** Destructor. */
    ~AsyncFunctionCaller() override
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
    MessageThreadCallback() = default;
    ~MessageThreadCallback() override       { cancelPendingUpdate(); }

    /** Returns true if the callback has completed. */
    bool hasFinished() const noexcept       { return finished; }

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

            if (job->shouldExit())
            {
                hasBeenCancelled = true;
                cancelPendingUpdate();
            }
            
            return;
        }

        if (auto thread = juce::Thread::getCurrentThread())
        {
            while (! (thread->threadShouldExit() || hasFinished()))
                waiter.wait (50);
            
            if (thread->threadShouldExit())
            {
                hasBeenCancelled = true;
                cancelPendingUpdate();
            }

            return;
        }

        TRACKTION_LOG_ERROR ("Rogue call to triggerAndWaitForCallback()");
        jassertfalse;
        waiter.wait (50);

        if (! hasFinished())
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("triggerAndWaitForCallback() unable to complete");
        }
    }

    virtual void performAction() = 0;

private:
    std::atomic<bool> finished { false }, hasBeenCancelled { false };
    juce::WaitableEvent waiter;

    void handleAsyncUpdate() override
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD
        
        if (hasBeenCancelled)
            return;

        performAction();
        finished = true;
        waiter.signal();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessageThreadCallback)
};

//==============================================================================
struct BlockingFunction  : public MessageThreadCallback
{
    BlockingFunction (std::function<void()> f)  : fn (f) {}

private:
    std::function<void()> fn;
    void performAction() override    { fn(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockingFunction)
};

inline bool callBlocking (std::function<void()> f)
{
    BlockingFunction bf (f);
    bf.triggerAndWaitForCallback();
    return bf.hasFinished();
}

}} // namespace tracktion { inline namespace engine
