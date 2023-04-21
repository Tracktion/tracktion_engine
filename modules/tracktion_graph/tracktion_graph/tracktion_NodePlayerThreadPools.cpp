/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
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

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
namespace
{
    constexpr int timeOutMilliseconds = -1;

    inline void pause()
    {
       #if JUCE_INTEL
        _mm_pause();
        _mm_pause();
       #else
        __asm__ __volatile__ ("yield");
        __asm__ __volatile__ ("yield");
       #endif
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

    void createThreads (size_t numThreads) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
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

    bool shouldWaitOrIsNotTriggered()
    {
        if (! triggered.load (std::memory_order_acquire))
            return false;
        
        return shouldWait();
    }
    
    void runThread()
    {
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

    void createThreads (size_t numThreads) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
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

    void runThread()
    {
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
       #if JUCE_INTEL
        _mm_pause();
        _mm_pause();
       #else
        __asm__ __volatile__ ("yield");
        __asm__ __volatile__ ("yield");
       #endif
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

    void createThreads (size_t numThreads) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
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

    bool shouldWaitOrIsNotTriggered()
    {
        if (! triggered.load (std::memory_order_acquire))
            return false;
        
        return shouldWait();
    }
    
    void runThread()
    {
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
template<typename SemaphoreType>
struct ThreadPoolSem : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolSem (LockFreeMultiThreadedNodePlayer& p)
        : ThreadPool (p)
    {
    }

    void createThreads (size_t numThreads) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        semaphore = std::make_unique<SemaphoreType> ((int) numThreads);

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

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
        if (semaphore) semaphore->signal (std::min (numToSignal, (int) threads.size()));
    }

    void signalAll() override
    {
        if (semaphore) semaphore->signal ((int) threads.size());
    }

    void wait()
    {
        if (! shouldWait())
            return;

        if (timeOutMilliseconds < 0)
        {
            semaphore->wait();
        }
        else
        {
            using namespace std::chrono;
            semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
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
    std::unique_ptr<SemaphoreType> semaphore;

    void runThread()
    {
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
template<typename SemaphoreType>
struct ThreadPoolSemHybrid : public LockFreeMultiThreadedNodePlayer::ThreadPool
{
    ThreadPoolSemHybrid (LockFreeMultiThreadedNodePlayer& p)
        : ThreadPool (p)
    {
    }

    void createThreads (size_t numThreads) override
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();
        semaphore = std::make_unique<SemaphoreType> ((int) numThreads);

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
        }
    }

    void clearThreads() override
    {
        signalShouldExit();

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
        if (semaphore) semaphore->signal (std::min (numToSignal, (int) threads.size()));
    }

    void signalAll() override
    {
        if (semaphore) semaphore->signal ((int) threads.size());
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
                if (timeOutMilliseconds < 0)
                {
                    semaphore->wait();
                }
                else
                {
                    using namespace std::chrono;
                    semaphore->timed_wait ((std::uint64_t) duration_cast<microseconds> (milliseconds (timeOutMilliseconds)).count());
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
        if (isFinalNodeReady())
            return;

        if (! shouldWait())
            return;

        pause();
        return;
    }

private:
    std::vector<std::thread> threads;
    std::unique_ptr<SemaphoreType> semaphore;

    void runThread()
    {
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
        case ThreadPoolStrategy::realTime:
        default:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolRT> (p); };
    }
}


#ifdef _MSC_VER
 #pragma warning (pop)
#endif

}}
