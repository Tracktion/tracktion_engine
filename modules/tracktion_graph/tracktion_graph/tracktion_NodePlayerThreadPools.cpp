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
/** Counts the threads that are sleeping, or about to, so that only those are
    signalled. A thread adds itself before it looks for Nodes one last time so
    a signal can't be missed, and each signal is for one of the threads counted
    here, so nothing is left on the semaphore for a thread to wake up to later.
*/
struct SleepingThreads
{
    /** A thread must call this before its last look for Nodes. */
    void add()
    {
        numSleeping.fetch_add (1);
    }

    /** A thread must call this if that last look found Nodes. If it returns
        false a signal has already been sent for the thread, which has to wait
        for it.
    */
    bool tryToRemove()
    {
        for (auto n = numSleeping.load();;)
        {
            if (n <= 0)
                return false;

            if (numSleeping.compare_exchange_weak (n, n - 1))
                return true;
        }
    }

    /** Returns the number of threads to signal, which will be no more than maxNum. */
    int claim (int maxNum)
    {
        // The Nodes that have just been queued have to be visible to any
        // thread that adds itself after the count is read here
        std::atomic_thread_fence (std::memory_order_seq_cst);

        for (auto n = numSleeping.load();;)
        {
            const auto numToClaim = std::min (n, maxNum);

            if (numToClaim <= 0)
                return 0;

            if (numSleeping.compare_exchange_weak (n, n - numToClaim))
                return numToClaim;
        }
    }

    /** Call this when there are no threads running. */
    void reset()
    {
        numSleeping = 0;
    }

    std::atomic<int> numSleeping { 0 };
};

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
    bool yieldBeforeSleeping = true;    // Only used by ThreadPoolSemHybrid
    bool wakeWhenNodesAreQueued = false;    // Wakes threads when Nodes are queued part way through a block
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
        sleepingThreads.reset();

        // When only sleeping threads are signalled there mustn't be any counts
        // on the semaphore that a thread could wake up to without being signalled
        semaphore = std::make_unique<SemaphoreType> (options.wakeWhenNodesAreQueued ? 0 : (int) numThreads, options.semaphoreSpins);
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
        signal (1);
    }

    void signal (int numToSignal) override
    {
        if (! semaphore)
            return;

        const auto num = std::min (numToSignal, (int) threads.size());

        if (options.wakeWhenNodesAreQueued)
        {
            signalSleepingThreads (num);
            return;
        }

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
        if (! semaphore)
            return;

        // A signal that isn't for a counted thread would be left on the semaphore
        if (options.wakeWhenNodesAreQueued)
            signalSleepingThreads ((int) threads.size());
        else
            semaphore->signal ((int) threads.size());
    }

    void nodesQueuedDuringBlock (int numNodes) override
    {
        if (options.wakeWhenNodesAreQueued && semaphore)
            signalSleepingThreads (numNodes);
    }

    /** Signals up to maxNum of the threads that are sleeping, and none that aren't. */
    void signalSleepingThreads (int maxNum)
    {
        if (const auto numToSignal = sleepingThreads.claim (maxNum); numToSignal > 0)
            semaphore->signal (numToSignal);
    }

    /** Returns false if Nodes were queued before the thread could sleep. */
    bool sleepOnSemaphore()
    {
        if (options.wakeWhenNodesAreQueued)
        {
            sleepingThreads.add();

            // If Nodes have been queued since the thread last looked, it only
            // sleeps if a signal has already been sent for it
            if (! shouldWait() && sleepingThreads.tryToRemove())
                return false;

            semaphore->wait();
            return true;
        }

        if (timeOutMilliseconds < 0)
        {
            semaphore->wait();
        }
        else
        {
            using namespace std::chrono;
            semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
        }

        return true;
    }

    bool wait()
    {
        if (! shouldWait())
            return false;

        if (! sleepOnSemaphore())
            return false;

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
    SleepingThreads sleepingThreads;
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
        sleepingThreads.reset();

        // When only sleeping threads are signalled there mustn't be any counts
        // on the semaphore that a thread could wake up to without being signalled
        semaphore = std::make_unique<SemaphoreType> (options.wakeWhenNodesAreQueued ? 0 : (int) numThreads, options.semaphoreSpins);
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
        signal (1);
    }

    void signal (int numToSignal) override
    {
        if (! semaphore)
            return;

        const auto num = std::min (numToSignal, (int) threads.size());

        if (options.wakeWhenNodesAreQueued)
        {
            signalSleepingThreads (num);
            return;
        }

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
        if (! semaphore)
            return;

        // A signal that isn't for a counted thread would be left on the semaphore
        if (options.wakeWhenNodesAreQueued)
            signalSleepingThreads ((int) threads.size());
        else
            semaphore->signal ((int) threads.size());
    }

    void nodesQueuedDuringBlock (int numNodes) override
    {
        if (options.wakeWhenNodesAreQueued && semaphore)
            signalSleepingThreads (numNodes);
    }

    /** Signals up to maxNum of the threads that are sleeping, and none that aren't. */
    void signalSleepingThreads (int maxNum)
    {
        if (const auto numToSignal = sleepingThreads.claim (maxNum); numToSignal > 0)
            semaphore->signal (numToSignal);
    }

    /** Returns false if Nodes were queued before the thread could sleep. */
    bool sleepOnSemaphore()
    {
        if (options.wakeWhenNodesAreQueued)
        {
            sleepingThreads.add();

            // If Nodes have been queued since the thread last looked, it only
            // sleeps if a signal has already been sent for it
            if (! shouldWait() && sleepingThreads.tryToRemove())
                return false;

            semaphore->wait();
            return true;
        }

        if (timeOutMilliseconds < 0)
        {
            semaphore->wait();
        }
        else
        {
            using namespace std::chrono;
            semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
        }

        return true;
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
                if (options.yieldBeforeSleeping)
                    std::this_thread::yield();
                else
                    pause();
            }
            else
            {
                pauseCount = 0;

                // Fall back to locking
                if (! sleepOnSemaphore())
                    return false;

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
    SleepingThreads sleepingThreads;
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

    ThreadPoolBlockEvent (LockFreeMultiThreadedNodePlayer& p, Idle i, bool wakeWhenNodesAreQueuedToUse = false)
        : ThreadPool (p), idle (i), wakeWhenNodesAreQueued (wakeWhenNodesAreQueuedToUse)
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

    void nodesQueuedDuringBlock (int) override
    {
        if (wakeWhenNodesAreQueued)
            signalAll();
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
    const bool wakeWhenNodesAreQueued;
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
        case ThreadPoolStrategy::lightweightSemaphoreWakeQueued:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSem<LightweightSemaphore>> (p, SemPoolOptions { .wakeWhenNodesAreQueued = true }); };
        case ThreadPoolStrategy::lightweightSemHybridWakeAll:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::all }); };
        case ThreadPoolStrategy::lightweightSemHybridCascade:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::cascade }); };
        case ThreadPoolStrategy::lightweightSemHybridCapped:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .wake = SemPoolOptions::Wake::capped }); };
        case ThreadPoolStrategy::lightweightSemHybridNoSemSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .semaphoreSpins = 0 }); };
        case ThreadPoolStrategy::lightweightSemHybridPause:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .yieldBeforeSleeping = false }); };
        case ThreadPoolStrategy::lightweightSemaphoreNoSemSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSem<LightweightSemaphore>> (p, SemPoolOptions { .semaphoreSpins = 0 }); };
        case ThreadPoolStrategy::blockEvent:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::sleep); };
        case ThreadPoolStrategy::blockEventHybrid:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::spinDuringBlock); };
        case ThreadPoolStrategy::blockEventSpin:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::spin); };
        case ThreadPoolStrategy::lightweightSemHybridPauseWakeQueued:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolSemHybrid<LightweightSemaphore>> (p, SemPoolOptions { .yieldBeforeSleeping = false, .wakeWhenNodesAreQueued = true }); };
        case ThreadPoolStrategy::blockEventWakeQueued:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolBlockEvent> (p, ThreadPoolBlockEvent::Idle::sleep, true); };
        case ThreadPoolStrategy::realTime:
        default:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolRT> (p); };
    }
}


#ifdef _MSC_VER
 #pragma warning (pop)
#endif

}
