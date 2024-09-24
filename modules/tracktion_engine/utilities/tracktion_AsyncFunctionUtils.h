/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_Threads.h"

namespace tracktion { inline namespace engine
{

/**
    Asyncronously call a function.
*/
struct AsyncCaller  : public juce::AsyncUpdater
{
    /** Creates an empty AsyncCaller. */
    AsyncCaller() = default;

    /** Creates an AsyncCaller with a given callback function. */
    AsyncCaller (std::function<void()> f)
    {
        function = std::move (f);
    }

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

    /** If an update has been triggered and is pending, this will invoke it
        synchronously.

        Use this as a kind of "flush" operation - if an update is pending, the
        handleAsyncUpdate() method will be called immediately; if no update is
        pending, then nothing will be done.

        Because this may invoke the callback, this method must only be called on
        the main event thread.
    */
    void handleUpdateNowIfNeeded()
    {
        juce::AsyncUpdater::handleUpdateNowIfNeeded();
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

    template<typename DurationType>
    void startTimer (std::chrono::duration<DurationType> interval)
    {
        juce::Timer::startTimer (static_cast<int> (std::chrono::duration_cast<std::chrono::milliseconds> (interval).count()));
    }

    using juce::Timer::startTimer;

    void timerCallback() override
    {
        if (callback)
            callback();
    }

private:
    std::function<void()> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LambdaTimer)
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

        if (isCurrentThreadSupplyingExitStatus())
        {
            while (! (shouldCurrentThreadExit() || hasFinished()))
                waiter.wait (50);

            if (shouldCurrentThreadExit())
            {
                hasBeenCancelled = true;
                cancelPendingUpdate();
            }

            return;
        }

        // If you get a deadlock here, it's probably because your MessageManager isn't
        // actually running and dispatching messages. This shouldn't be called with a
        // blocked message manager
        waiter.wait();

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

/** Calls a function on the message thread by posting a message and then waiting
    for it to be delivered. If this fails for some reason, e.g. the calling thread
    is trying to exit or is blocking the message thread, this will throw an
    exception.
*/
inline bool callBlocking (std::function<void()> f)
{
    BlockingFunction bf (f);
    bf.triggerAndWaitForCallback();

    if (! bf.hasFinished())
        throw std::runtime_error ("Blocking function unable to complete");

    return true;
}


}} // namespace tracktion { inline namespace engine
