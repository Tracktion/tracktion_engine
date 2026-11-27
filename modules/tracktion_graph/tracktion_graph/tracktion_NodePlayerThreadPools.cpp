/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <thread>

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

namespace tracktion::inline graph {

//==============================================================================
//==============================================================================
namespace
{
    constexpr int timeOutMilliseconds = -1;

    /** Call this from each pool thread when it starts.
        These have to be set by the thread itself as a thread can only be
        upgraded to real-time from within it, and that has to happen after its
        priority is set or it would be replaced.
    */
    inline void setUpThreadPriority (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setCurrentThreadPriority (10);
        tryToUpgradeCurrentThreadToRealtime (rtOpts);
    }

    inline void pause()
    {
        tracktion::core::pause();
        tracktion::core::pause();
    }
}


//==============================================================================
//==============================================================================
struct ThreadPoolCV : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolCV (LockFreeMultiThreadedNodePlayer& p)
        : ThreadPool (p)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                      .withPriority (10)
                      .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    void signalOne() override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        condition.notify_one();
    }

    void signal (int numToSignal) override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        for (int i = std::min ((int) threads.size(), numToSignal); --i >= 0;)
             condition.notify_one();
    }

    void signalAll() override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        condition.notify_all();
    }

    void wait()
    {
        if (! shouldWait())
            return;

        std::unique_lock<std::mutex> lock (mutex);

        if (timeOutMilliseconds < 0)
        {
            condition.wait (lock, [this] { return ! shouldWait(); });
        }
        else
        {
            if (! condition.wait_for (lock, std::chrono::milliseconds (timeOutMilliseconds),
                                      [this] { return ! shouldWait(); }))
            {
                return;
            }
        }
    }

    void waitForFinalNode() override
    {
        if (isFinalNodeReady())
            return;

        if (! shouldWait())
            return;

        pause();
        return;
    }

private:
    std::vector<std::thread> threads;
    mutable std::mutex mutex;
    mutable std::condition_variable condition;
    mutable std::atomic<bool> triggered { false };
    juce::AudioWorkgroup workgroup;

    bool shouldWaitOrIsNotTriggered()
    {
        if (! triggered.load (std::memory_order_acquire))
            return false;

        return shouldWait();
    }

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);

        for (;;)
        {
            if (shouldExit())
                return;

            if (! process())
                wait();
        }
    }
};


//==============================================================================
//==============================================================================
struct ThreadPoolRT : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolRT (LockFreeMultiThreadedNodePlayer& p)
        : ThreadPool (p)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                      .withPriority (10)
                      .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    void signalOne() override
    {
    }

    void signal (int) override
    {
    }

    void signalAll() override
    {
    }

    void wait()
    {
        // The pause and sleep counts avoid starving the CPU if there aren't enough queued nodes
        // This only happens on the worker threads so the main audio thread never interacts with the thread scheduler
        thread_local int pauseCount = 0;

        if (shouldWait())
        {
            ++pauseCount;

            if (pauseCount < 50)
                pause();
            else if (pauseCount < 100)
                std::this_thread::yield();
            else if (pauseCount < 150)
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            else if (pauseCount < 200)
                std::this_thread::sleep_for (std::chrono::milliseconds (5));
            else
                pauseCount = 0;
        }
        else
        {
            pauseCount = 0;
        }
    }

    void waitForFinalNode() override
    {
        pause();
    }

private:
    std::vector<std::thread> threads;
    juce::AudioWorkgroup workgroup;

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);

        for (;;)
        {
            if (shouldExit())
                return;

            if (! process())
                wait();
        }
    }

    inline void pause()
    {
        tracktion::core::pause();
        tracktion::core::pause();
    }
};


//==============================================================================
//==============================================================================
struct ThreadPoolHybrid : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolHybrid (LockFreeMultiThreadedNodePlayer& p)
        : ThreadPool (p)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                              .withPriority (10)
                              .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    void signalOne() override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        condition.notify_one();
    }

    void signal (int numToSignal) override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        for (int i = std::min ((int) threads.size(), numToSignal); --i >= 0;)
             condition.notify_one();
    }

    void signalAll() override
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        condition.notify_all();
    }

    void wait()
    {
        thread_local int pauseCount = 0;

        if (shouldExit())
            return;

        if (shouldWait())
        {
            ++pauseCount;

            if (pauseCount < 25)
            {
                pause();
            }
            else if (pauseCount < 50)
            {
                std::this_thread::yield();
            }
            else
            {
                pauseCount = 0;

                // Fall back to locking
                std::unique_lock<std::mutex> lock (mutex);

                if (timeOutMilliseconds < 0)
                {
                    condition.wait (lock, [this] { return ! shouldWaitOrIsNotTriggered(); });
                }
                else
                {
                    if (! condition.wait_for (lock, std::chrono::milliseconds (timeOutMilliseconds),
                                              [this] { return ! shouldWaitOrIsNotTriggered(); }))
                    {
                        return;
                    }
                }
            }
        }
        else
        {
            pauseCount = 0;
        }
    }

    void waitForFinalNode() override
    {
        pause();
    }

private:
    std::vector<std::thread> threads;
    mutable std::mutex mutex;
    mutable std::condition_variable condition;
    mutable std::atomic<bool> triggered { false };
    juce::AudioWorkgroup workgroup;


    bool shouldWaitOrIsNotTriggered()
    {
        if (! triggered.load (std::memory_order_acquire))
            return false;

        return shouldWait();
    }

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);

        for (;;)
        {
            if (shouldExit())
                return;

            if (! process())
                wait();
        }
    }
};


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
/** Options for the semaphore based pools so their variants can be compared. */
struct SemPoolOptions
{
    enum class Wake
    {
        each,       // Signals the semaphore once for each thread
        all,        // Wakes all the sleeping threads with a single call
        cascade,    // Wakes two threads, which each wake two more if there are Nodes queued
        capped      // As each, but doesn't let unused counts build up
    };

    Wake wake = Wake::each;
    int semaphoreSpins = 10'000;
};

template<typename SemaphoreType>
struct ThreadPoolSem : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolSem (LockFreeMultiThreadedNodePlayer& p, SemPoolOptions o = {})
        : ThreadPool (p), options (o)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        semaphore = std::make_unique<SemaphoreType> ((int) numThreads, options.semaphoreSpins);
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                      .withPriority (10)
                      .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

        // Wakes any threads that missed a semaphore_signal_all
        if (semaphore && options.wake == SemPoolOptions::Wake::all)
            semaphore->wakeAllWaiters();

        for (auto& t : threads)
            t.join();

        threads.clear();
        semaphore.reset();
    }

    void signalOne() override
    {
        if (semaphore) semaphore->signal();
    }

    void signal (int numToSignal) override
    {
        if (! semaphore)
            return;

        const auto num = std::min (numToSignal, (int) threads.size());

        switch (options.wake)
        {
            case SemPoolOptions::Wake::all:     semaphore->signalWakingAll (num); break;
            case SemPoolOptions::Wake::cascade: semaphore->signal (std::min (num, 2)); break;
            case SemPoolOptions::Wake::capped:  semaphore->signalCapped (num); break;
            case SemPoolOptions::Wake::each:    semaphore->signal (num); break;
        }
    }

    void wakeMoreThreads()
    {
        if (options.wake != SemPoolOptions::Wake::cascade)
            return;

        // This thread is about to take one of the queued Nodes so only wake others if there are more
        if (const auto numSpareNodes = getNumNodesQueued() - 1; numSpareNodes > 0)
            semaphore->signal (std::min (numSpareNodes, 2));
    }

    void signalAll() override
    {
        if (semaphore) semaphore->signal ((int) threads.size());
    }

    bool wait()
    {
        if (! shouldWait())
            return false;

        if (timeOutMilliseconds < 0)
        {
            semaphore->wait();
        }
        else
        {
            using namespace std::chrono;
            semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
        }

        wake_experiment::count (wake_experiment::numWakes);
        wakeMoreThreads();
        return true;
    }

    void waitForFinalNode() override
    {
        if (isFinalNodeReady())
            return;

        if (! shouldWait())
            return;

        pause();
        return;
    }

private:
    const SemPoolOptions options;
    std::vector<std::thread> threads;
    std::unique_ptr<SemaphoreType> semaphore;
    juce::AudioWorkgroup workgroup;

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);

        bool justWoke = false;

        for (;;)
        {
            if (shouldExit())
                return;

            if (process())
            {
                justWoke = false;
            }
            else
            {
                if (justWoke)
                    wake_experiment::count (wake_experiment::numEmptyWakes);

                justWoke = wait();
            }
        }
    }
};


//==============================================================================
//==============================================================================
template<typename SemaphoreType>
struct ThreadPoolSemHybrid : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolSemHybrid (LockFreeMultiThreadedNodePlayer& p, SemPoolOptions o = {})
        : ThreadPool (p), options (o)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        semaphore = std::make_unique<SemaphoreType> ((int) numThreads, options.semaphoreSpins);
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                      .withPriority (10)
                      .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

        // Wakes any threads that missed a semaphore_signal_all
        if (semaphore && options.wake == SemPoolOptions::Wake::all)
            semaphore->wakeAllWaiters();

        for (auto& t : threads)
            t.join();

        threads.clear();
        semaphore.reset();
    }

    void signalOne() override
    {
        if (semaphore) semaphore->signal();
    }

    void signal (int numToSignal) override
    {
        if (! semaphore)
            return;

        const auto num = std::min (numToSignal, (int) threads.size());

        switch (options.wake)
        {
            case SemPoolOptions::Wake::all:     semaphore->signalWakingAll (num); break;
            case SemPoolOptions::Wake::cascade: semaphore->signal (std::min (num, 2)); break;
            case SemPoolOptions::Wake::capped:  semaphore->signalCapped (num); break;
            case SemPoolOptions::Wake::each:    semaphore->signal (num); break;
        }
    }

    void wakeMoreThreads()
    {
        if (options.wake != SemPoolOptions::Wake::cascade)
            return;

        // This thread is about to take one of the queued Nodes so only wake others if there are more
        if (const auto numSpareNodes = getNumNodesQueued() - 1; numSpareNodes > 0)
            semaphore->signal (std::min (numSpareNodes, 2));
    }

    void signalAll() override
    {
        if (semaphore) semaphore->signal ((int) threads.size());
    }

    bool wait()
    {
        thread_local int pauseCount = 0;

        if (shouldExit())
            return false;

        if (shouldWait())
        {
            ++pauseCount;

            if (pauseCount < 25)
            {
                pause();
            }
            else if (pauseCount < 50)
            {
                std::this_thread::yield();
            }
            else
            {
                pauseCount = 0;

                // Fall back to locking
                if (timeOutMilliseconds < 0)
                {
                    semaphore->wait();
                }
                else
                {
                    using namespace std::chrono;
                    semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
                }

                wake_experiment::count (wake_experiment::numWakes);
                wakeMoreThreads();
                return true;
            }
        }
        else
        {
            pauseCount = 0;
        }

        return false;
    }

    void waitForFinalNode() override
    {
        if (isFinalNodeReady())
            return;

        if (! shouldWait())
            return;

        pause();
        return;
    }

private:
    const SemPoolOptions options;
    std::vector<std::thread> threads;
    std::unique_ptr<SemaphoreType> semaphore;
    juce::AudioWorkgroup workgroup;

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);
        
        juce::FloatVectorOperations::disableDenormalisedNumberSupport();

        bool justWoke = false;

        for (;;)
        {
            if (shouldExit())
                return;

            if (process())
            {
                justWoke = false;
            }
            else
            {
                if (justWoke)
                    wake_experiment::count (wake_experiment::numEmptyWakes);

                justWoke = wait();
            }
        }
    }
};
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
/** Threads sleep on a counter which the audio thread changes at the start of
    each block. As a thread only sleeps if the counter hasn't changed since it
    last looked for Nodes, a wake-up can't be missed and nothing needs to be
    counted, so all the threads are woken with a single call.
*/
struct ThreadPoolBlockEvent : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    enum class Idle
    {
        sleep,              // Sleep as soon as there are no Nodes queued
        spinDuringBlock,    // Look for Nodes until the block has finished, then sleep
        spin                // Never sleep
    };

    ThreadPoolBlockEvent (LockFreeMultiThreadedNodePlayer& p, Idle i)
        : ThreadPool (p), idle (i)
    {
    }

    void createThreads (size_t numThreads, juce::AudioWorkgroup workgroupToUse) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        workgroup = workgroupToUse;

        const auto rtOpts = juce::Thread::RealtimeOptions()
                              .withPriority (10)
                              .withApproximateAudioProcessingTime (player.getBlockSize(), player.getSampleRate());

        for (size_t i = 0; i < numThreads; ++i)
            threads.emplace_back ([this, rtOpts] { runThread (rtOpts); });
    }

    void clearThreads() override
    {
        signalShouldExit();

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    void signalOne() override               { signalAll(); }
    void signal (int) override              { signalAll(); }

    void signalAll() override
    {
        blockCounter.fetch_add (1, std::memory_order_release);

        if (idle != Idle::spin)
            blockCounter.notify_all();
    }

    void waitForFinalNode() override
    {
        if (isFinalNodeReady())
            return;

        if (! shouldWait())
            return;

        pause();
    }

private:
    const Idle idle;
    std::vector<std::thread> threads;
    juce::AudioWorkgroup workgroup;
    std::atomic<uint32_t> blockCounter { 0 };

    /** Returns true if the thread slept. */
    bool wait()
    {
        thread_local int pauseCount = 0;

        if (! shouldWait())
        {
            pauseCount = 0;
            return false;
        }

        if (idle == Idle::spin || (idle == Idle::spinDuringBlock && isBlockInProgress()))
        {
            if (++pauseCount < 25)
                pause();
            else
                std::this_thread::yield();

            return false;
        }

        pauseCount = 0;
        const auto counterBeforeLooking = blockCounter.load (std::memory_order_acquire);

        // Look again now the counter has been read as if Nodes have been queued
        // since, the counter may already have changed and the wait would return
        if (! shouldWait())
            return false;

        blockCounter.wait (counterBeforeLooking, std::memory_order_acquire);
        wake_experiment::count (wake_experiment::numWakes);
        return true;
    }

    void runThread (const juce::Thread::RealtimeOptions& rtOpts)
    {
        setUpThreadPriority (rtOpts);

        juce::WorkgroupToken token;
        workgroup.join (token);

        juce::FloatVectorOperations::disableDenormalisedNumberSupport();
        bool justWoke = false;

        for (;;)
        {
            if (shouldExit())
                return;

            if (process())
            {
                justWoke = false;
            }
            else
            {
                if (justWoke)
                    wake_experiment::count (wake_experiment::numEmptyWakes);

                justWoke = wait();
            }
        }
    }
};


LockFreeMultiThreadedNodePlayer::ThreadPoolCreator getPoolCreatorFunction (ThreadPoolStrategy poolType)
{
    switch (poolType)
    {
        case ThreadPoolStrategy::conditionVariable:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolCV> (p); };
        case ThreadPoolStrategy::hybrid:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolHybrid> (p); };
        case ThreadPoolStrategy::semaphore:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSem<Semaphore>> (p); };
        case ThreadPoolStrategy::lightweightSemaphore:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSem<LightweightSemaphore>> (p); };
        case ThreadPoolStrategy::lightweightSemHybrid:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p); };
        case ThreadPoolStrategy::lightweightSemHybridWakeAll:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::all }); };
        case ThreadPoolStrategy::lightweightSemHybridCascade:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::cascade }); };
        case ThreadPoolStrategy::lightweightSemHybridCapped:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::capped }); };
        case ThreadPoolStrategy::lightweightSemHybridNoSemSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .semaphoreSpins = 0 }); };
        case ThreadPoolStrategy::lightweightSemaphoreNoSemSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSem<LightweightSemaphore>> (p, SemPoolOptions { .semaphoreSpins = 0 }); };
        case ThreadPoolStrategy::blockEvent:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::sleep); };
        case ThreadPoolStrategy::blockEventHybrid:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::spinDuringBlock); };
        case ThreadPoolStrategy::blockEventSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::spin); };
        case ThreadPoolStrategy::realTime:
        default:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolRT> (p); };
    }
}


#ifdef _MSC_VER
 #pragma warning (pop)
#endif

}
