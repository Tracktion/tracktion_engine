/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_graph
{

#if GRAPH_UNIT_TESTS_SEMAPHORE

class SemaphoreTests    : public juce::UnitTest
{
public:
    SemaphoreTests()
        : juce::UnitTest ("Semaphore", "tracktion_graph") {}

    //==============================================================================
    void runTest() override
    {
        runSemaphoreTests<Semaphore> ("Semaphore");
        runSemaphoreTests<LightweightSemaphore> ("LightweightSemaphore");
    }
    
private:
    template<typename SemaphoreType>
    void runSemaphoreTests (juce::String semaphoreName)
    {
        beginTest (juce::String ("Semaphore basic tests").replace ("Semaphore", semaphoreName));
        {
            {
                SemaphoreType event (1);
                expect (event.wait()); // Count of 1 doesn't block
            }

            {
                SemaphoreType event (2);
                event.wait();
                expect (event.timed_wait (100)); // Count of 2 doesn't block
                // expect (! event.wait()); // 3rd wait would block
            }

            {
                SemaphoreType event (2);
                expect (event.wait());
                expect (event.wait());
                expect (! event.try_wait()); // 3rd wait fails
                expect (! event.timed_wait (100)); // Timed wait fails
            }
        }
        
        beginTest (juce::String ("Semaphore wakeup tests").replace ("Semaphore", semaphoreName));
        {
            constexpr int numThreads = 10;
            std::atomic<int> counter { 0 }, numThreadsRunning { 0 };
            SemaphoreType event;
            auto signalTime = std::chrono::steady_clock::now();
            
            // Start all the threads
            std::vector<std::thread> threads;
            
            for (int i = 0; i < numThreads; ++i)
            {
                threads.emplace_back ([&, this]
                                      {
                                          ++numThreadsRunning;
                                          event.wait();
                                          
                                          auto signalDuration = std::chrono::steady_clock::now() - signalTime;
                                          logMessage (juce::String (std::chrono::duration_cast<std::chrono::microseconds> (signalDuration).count()) + "us");

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
            
            expectEquals (counter.load(), numThreads);
        }
    }
};

static SemaphoreTests semaphoreTests;

#endif

} // namespace tracktion_engine
