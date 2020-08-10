/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <thread>

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

namespace tracktion_graph
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
        #warning Implement for ARM
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
        condition.notify_one();
    }

    void signalAll() override
    {
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
        #warning Implement for ARM
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
        condition.notify_one();
    }

    void signalAll() override
    {
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
        case ThreadPoolStrategy::realTime:
        default:
            return [] (LockFreeMultiThreadedNodePlayer& p) { return std::make_unique<ThreadPoolRT> (p); };
    }
}


#ifdef _MSC_VER
 #pragma warning (pop)
#endif

}
