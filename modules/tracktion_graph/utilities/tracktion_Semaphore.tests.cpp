/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS && GRAPH_BENCHMARKS_THREADS
 #include "../../tracktion_core/utilities/tracktion_Benchmark.h"
#endif

namespace tracktion::inline graph {

} // namespace tracktion::inline graph

#if GRAPH_UNIT_TESTS_SEMAPHORE

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

namespace
{
    template<typename SemaphoreType>
    void runSemaphoreTests (juce::String semaphoreName)
    {
        auto basicName = juce::String ("Semaphore basic tests").replace ("Semaphore", semaphoreName).toStdString();
        SUBCASE (basicName.c_str())
        {
            {
                SemaphoreType event (1);
                CHECK (event.wait()); // Count of 1 doesn't block
            }

            {
                SemaphoreType event (2);
                event.wait();
                CHECK (event.timed_wait (100)); // Count of 2 doesn't block
                // CHECK (! event.wait()); // 3rd wait would block
            }

            {
                SemaphoreType event (2);
                CHECK (event.wait());
                CHECK (event.wait());
                CHECK (! event.try_wait()); // 3rd wait fails
                CHECK (! event.timed_wait (100)); // Timed wait fails
            }
        }

        auto wakeupName = juce::String ("Semaphore wakeup tests").replace ("Semaphore", semaphoreName).toStdString();
        SUBCASE (wakeupName.c_str())
        {
            constexpr int numThreads = 10;
            std::atomic<int> counter { 0 }, numThreadsRunning { 0 };
            SemaphoreType event;
            auto signalTime = std::chrono::steady_clock::now();

            // Start all the threads
            std::vector<std::thread> threads;

            for (int i = 0; i < numThreads; ++i)
            {
                threads.emplace_back ([&, signalTime]
                                      {
                                          ++numThreadsRunning;
                                          event.wait();

                                          auto signalDuration = std::chrono::steady_clock::now() - signalTime;
                                          MESSAGE ((juce::String (std::chrono::duration_cast<std::chrono::microseconds> (signalDuration).count()) + "us").toStdString());

                                          ++counter;
                                      });
            }

            // Wait until all the threads have started and are waiting on the event
            for (;;)
            {
                if (numThreadsRunning == numThreads)
                    break;

                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }

            // Sleep for a few more ms to ensure they're all waiting
            std::this_thread::sleep_for (std::chrono::milliseconds (5));

            // Signal all the threads to increment the counter
            signalTime = std::chrono::steady_clock::now();
            event.signal (numThreads);

            // Wait for the threads to complete
            for (auto& t : threads)
                t.join();

            CHECK_EQ (counter.load(), numThreads);
        }
    }
}

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("Semaphore")
    {
        runSemaphoreTests<Semaphore> ("Semaphore");
        runSemaphoreTests<LightweightSemaphore> ("LightweightSemaphore");
    }
}

} // namespace tracktion::inline graph

#endif

#if TRACKTION_BENCHMARKS && GRAPH_BENCHMARKS_THREADS

namespace tracktion::inline graph {

//==============================================================================
//==============================================================================
class NoOpBenchmarks    : public juce::UnitTest
{
public:
    NoOpBenchmarks()
        : juce::UnitTest ("no_op", "tracktion_benchmarks") {}

    //==============================================================================
    void runTest() override
    {
        Benchmark benchmark (createBenchmarkDescription ("Time", "no_op", "no_op"));

        for (int i = 0; i < 100'000; ++i)
        {
            benchmark.start();
            benchmark.stop();
        }

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }
};

static NoOpBenchmarks noOpBenchmarks;

//==============================================================================
//==============================================================================
class ChronoNowBenchmarks   : public juce::UnitTest
{
public:
    ChronoNowBenchmarks()
        : juce::UnitTest ("steady_clock::now", "tracktion_benchmarks") {}

    //==============================================================================
    void runTest() override
    {
        Benchmark benchmark (createBenchmarkDescription ("Time", "steady_clock::now", "chrono::steady_clock::now"));

        for (int i = 0; i < 100'000; ++i)
        {
            benchmark.start();
            [[ maybe_unused ]] volatile auto now = std::chrono::steady_clock::now();
            benchmark.stop();
        }

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }
};

static ChronoNowBenchmarks chronoNowBenchmarks;

//==============================================================================
//==============================================================================
class JUCEMsCounterHiRes   : public juce::UnitTest
{
public:
    JUCEMsCounterHiRes()
        : juce::UnitTest ("juce::Time::getMillisecondCounterHiRes()", "tracktion_benchmarks") {}

    //==============================================================================
    void runTest() override
    {
        Benchmark benchmark (createBenchmarkDescription ("Time", "juce ms timer hi-res", "juce::Time::getMillisecondCounterHiRes()"));

        for (int i = 0; i < 100'000; ++i)
        {
            benchmark.start();
            [[ maybe_unused ]] volatile auto now = juce::Time::getMillisecondCounterHiRes();
            benchmark.stop();
        }

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }
};

static JUCEMsCounterHiRes juceMsCounterHiRes;

//==============================================================================
//==============================================================================
class ThreadSignallingBenchmarks    : public juce::UnitTest
{
public:
    ThreadSignallingBenchmarks()
        : juce::UnitTest ("Thread signalling", "tracktion_benchmarks") {}

    //==============================================================================
    void runTest() override
    {
        runConditionVariableBenchmarks();
        runSemaphoreBenchmarks<Semaphore> ("Semaphore");
        runSemaphoreBenchmarks<LightweightSemaphore> ("LightweightSemaphore");

        runNonWaitingConditionVariableBenchmarks();
        runNonWaitingSemaphoreBenchmarks<Semaphore> ("Semaphore");
        runNonWaitingSemaphoreBenchmarks<LightweightSemaphore> ("LightweightSemaphore");
    }

private:
    template<typename SemaphoreType>
    void runSemaphoreBenchmarks (juce::String semaphoreName)
    {
        constexpr int numThreads = 10;
        Benchmark benchmark (createBenchmarkDescription ("Threads",
                                                         juce::String ("Semaphore signal").replace ("Semaphore", semaphoreName).toStdString(),
                                                         juce::String ("Signal numThreads from waiting").replace ("numThreads", juce::String (numThreads)).toStdString()));

        using namespace std::literals;
        std::atomic<int> numThreadsRunning { 0 };
        SemaphoreType event;
        auto signalTime = std::chrono::steady_clock::now();

        // Start all the threads
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([&]
                                  {
                                      ++numThreadsRunning;
                                      event.wait();
                                  });
        }

        // Wait until all the threads have started and are waiting on the event
        for (;;)
        {
            if (numThreadsRunning == numThreads)
                break;

            std::this_thread::sleep_for (1ms);
        }

        // Sleep for a few more ms to ensure they're all waiting
        std::this_thread::sleep_for (5ms);

        // Signal all the threads
        signalTime = std::chrono::steady_clock::now();

        {
            // Signal all the threads
            signalTime = std::chrono::steady_clock::now();

            benchmark.start();
            event.signal (numThreads);
            benchmark.stop();
        }

        // Wait for the threads to complete
        for (auto& t : threads)
            t.join();

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }

    void runConditionVariableBenchmarks()
    {
        constexpr int numThreads = 10;
        Benchmark benchmark (createBenchmarkDescription ("Threads",
                                                         juce::String ("CV signal").toStdString(),
                                                         juce::String ("Signal numThreads from waiting").replace ("numThreads", juce::String (numThreads)).toStdString()));

        using namespace std::literals;
        std::atomic<int> numThreadsRunning { 0 };
        juce::WaitableEvent event (true);

        // Start all the threads
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([&]
                                  {
                                      ++numThreadsRunning;
                                      event.wait (-1);
                                  });
        }

        // Wait until all the threads have started and are waiting on the event
        for (;;)
        {
            if (numThreadsRunning == numThreads)
                break;

            std::this_thread::sleep_for (1ms);
        }

        // Sleep for a few more ms to ensure they're all waiting
        std::this_thread::sleep_for (5ms);

        {
            benchmark.start();

            // Signal all the threads
            event.signal();

            benchmark.stop();
        }

        // Wait for the threads to complete
        for (auto& t : threads)
            t.join();

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }

    template<typename SemaphoreType>
    void runNonWaitingSemaphoreBenchmarks (juce::String semaphoreName)
    {
        constexpr int numThreads = 10;
        using namespace std::literals;
        Benchmark benchmark (createBenchmarkDescription ("Threads",
                                                         juce::String ("Semaphore signal").replace ("Semaphore", semaphoreName).toStdString(),
                                                         juce::String ("Signal numThreads (may not be waiting)").replace ("numThreads", juce::String (numThreads)).toStdString()));

        std::atomic<int> numThreadsRunning { 0 };
        std::atomic<bool> threadShouldExit { false };
        SemaphoreType event;

        // Start all the threads
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([&]
                                  {
                                      ++numThreadsRunning;

                                      while (! threadShouldExit)
                                          event.wait();

                                      --numThreadsRunning;
                                  });
        }

        // Wait until all the threads have started and are waiting on the event
        for (;;)
        {
            if (numThreadsRunning == numThreads)
                break;

            std::this_thread::sleep_for (1ms);
        }

        // Sleep for a few more ms to ensure they're all waiting
        std::this_thread::sleep_for (5ms);

        for (int i = 0; i < 100'000; ++i)
        {
            benchmark.start();
            event.signal (numThreads);
            benchmark.stop();
        }

        // Wait for the threads to complete
        threadShouldExit = true;

        while (numThreadsRunning > 0)
            event.signal (numThreads);

        for (auto& t : threads)
            t.join();

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }

    void runNonWaitingConditionVariableBenchmarks()
    {
        constexpr int numThreads = 10;
        using namespace std::literals;
        Benchmark benchmark (createBenchmarkDescription ("Threads",
                                                         juce::String ("CV signal").toStdString(),
                                                         juce::String ("Signal numThreads (may not be waiting)").replace ("numThreads", juce::String (numThreads)).toStdString()));

        std::atomic<int> numThreadsRunning { 0 };
        std::atomic<bool> threadShouldExit { false };
        juce::WaitableEvent event;

        // Start all the threads
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([&]
                                  {
                                      ++numThreadsRunning;

                                      while (! threadShouldExit)
                                          event.wait();

                                      --numThreadsRunning;
                                  });
        }

        // Wait until all the threads have started and are waiting on the event
        for (;;)
        {
            if (numThreadsRunning == numThreads)
                break;

            std::this_thread::sleep_for (1ms);
        }

        // Sleep for a few more ms to ensure they're all waiting
        std::this_thread::sleep_for (5ms);

        for (int i = 0; i < 100'000; ++i)
        {
            benchmark.start();
            event.signal();
            benchmark.stop();
        }

        // Wait for the threads to complete
        threadShouldExit = true;

        while (numThreadsRunning > 0)
            event.signal();

        for (auto& t : threads)
            t.join();

        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }
};

static ThreadSignallingBenchmarks threadSignallingBenchmarks;

} // namespace tracktion::inline graph
#endif
