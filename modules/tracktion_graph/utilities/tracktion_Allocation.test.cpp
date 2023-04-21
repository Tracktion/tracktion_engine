/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "../../3rd_party/rpmalloc/rpallocator.h"


namespace tracktion { inline namespace graph
{

#if GRAPH_UNIT_TESTS_ALLOCATION

class AllocationTests    : public juce::UnitTest
{
public:
    AllocationTests()
        : juce::UnitTest ("Allocation", "tracktion_graph") {}

    //==============================================================================
    void runTest() override
    {
        runRPMallocTests();
    }
    
private:
    void runRPMallocTests()
    {
        beginTest ("rpmalloc single thread");
        {
            expectEquals (rpmalloc_initialize(), 0, "rpmalloc_initialize failed");
            
            {
                constexpr size_t numFloats = 256;
                constexpr size_t numBytes = numFloats * sizeof (float);
                auto data = static_cast<float*> (rpmalloc (numBytes));
                expect (data != nullptr);
                
                std::fill_n (data, numFloats, 0.7f);
                
                rpfree (data);
            }

            {
                constexpr size_t numFloats = 256;
                constexpr size_t numBytes = numFloats * sizeof (float);
                auto data = static_cast<float*> (rpmalloc (numBytes));
                expect (data != nullptr);
                
                std::fill_n (data, numFloats, 0.7f);
                
                rpfree (data);
            }

            rpmalloc_finalize();
        }

        beginTest ("rpmalloc multi thread");
        {
            expectEquals (rpmalloc_initialize(), 0, "rpmalloc_initialize failed");

            constexpr size_t numInts = 256;
            constexpr size_t numBytes = numInts * sizeof (float);
            int* data1 = nullptr, *data2 = nullptr;
            
            std::thread t1 ([&]
                            {
                                rpmalloc_thread_initialize();

                                data1 = static_cast<int*> (rpmalloc (numBytes));
                                expect (data1 != nullptr);
                                std::fill_n (data1, numInts, 42);
                                expectEquals (*data1, 42);
                                expectEquals (data1[numInts - 1], 42);
                                
                                rpmalloc_thread_finalize();
                            });
            
            std::thread t2 ([&]
                            {
                                t1.join();
                                rpmalloc_thread_initialize();

                                data2 = static_cast<int*> (rpmalloc (numBytes));
                                expect (data2 != nullptr);
                                std::fill_n (data2, numInts, 42);
                                expectEquals (*data2, 42);
                                expectEquals (data2[numInts - 1], 42);

                                expectEquals (*data1, 42);
                                expectEquals (data1[numInts - 1], 42);
                                rpfree (data1);
                                
                                rpmalloc_thread_finalize();
                            });
            
            t2.join();
            expectEquals (*data2, 42);
            expectEquals (data2[numInts - 1], 42);
            rpfree (data2);

            rpmalloc_finalize();
        }

        beginTest ("rpallocator");
        {
            std::vector<int, rpallocator<int>> intVec (1024, 0);
            std::fill (intVec.begin(), intVec.end(), 42);
            expectEquals (*intVec.begin(), 42);
            expectEquals (intVec[intVec.size() - 1], 42);

            intVec.push_back (43);
            expectEquals (*intVec.begin(), 42);
            expectEquals (intVec[intVec.size() - 1], 43);
        }

        beginTest ("rpallocator cross-thread");
        {
            std::vector<int, rpallocator<int>> intVec1 (1024, 1);

            std::thread t1 ([this, &intVec1]
                            {
                                expectEquals (*intVec1.begin(), 1);
                                expectEquals (intVec1[intVec1.size() - 1], 1);
                
                                std::vector<int, rpallocator<int>> intVec2 (1024, 2);
                                expectEquals (*intVec2.begin(), 2);
                                expectEquals (intVec2[intVec2.size() - 1], 2);

                                intVec1.push_back (43);
                                expectEquals (*intVec1.begin(), 1);
                                expectEquals (intVec1[intVec1.size() - 1], 43);
                            });

            std::thread t2 ([this, &intVec1, &t1]
                            {
                                t1.join();
                
                                intVec1.push_back (44);
                                expectEquals (*intVec1.begin(), 1);
                                expectEquals (intVec1[intVec1.size() - 1], 44);
                            });

            t2.join();
            intVec1.push_back (45);
            expectEquals (*intVec1.begin(), 1);
            expectEquals (intVec1[intVec1.size() - 3], 43);
            expectEquals (intVec1[intVec1.size() - 2], 44);
            expectEquals (intVec1[intVec1.size() - 1], 45);

            std::thread t3 ([this, &intVec1]
                            {
                                intVec1.clear();
                                expect (intVec1.size() == 0);
                                expect (intVec1.capacity() > 0);
                                intVec1.shrink_to_fit();
                                expect (intVec1.capacity() == 0);
                            });
            t3.join();
            expect (intVec1.size() == 0);
            expect (intVec1.capacity() == 0);
        }
        
        beginTest ("rpallocator multi-thread");
        {
            // Create 20 threads
            // Randomly push or pop ints in to a vector
            
            constexpr int numThreads = 20;
            std::vector<std::thread> pool;
            std::atomic<bool> shouldExit { false };

            for (int threadNum = 0; threadNum < numThreads; ++threadNum)
                pool.emplace_back ([&shouldExit]
                                   {
                                       const size_t maxSize = 100'000;
                                       juce::Random r;
                                       std::vector<int, rpallocator<int>> vec;
                    
                                       while (! shouldExit)
                                       {
                                           const bool remove = r.nextBool() || vec.size() >= maxSize;
                                           const auto num = (size_t) r.nextInt ({ 5, 6 + r.nextInt (5) });

                                           if (remove)
                                           {
                                               const auto numToRemove = std::min (num, vec.size());
                                               
                                               for (size_t i = 0; i < numToRemove; ++i)
                                                   vec.pop_back();
                                           }
                                           else
                                           {
                                               const auto numToAdd = (size_t) r.nextInt ({ 5, 6 + r.nextInt (5) });

                                               for (size_t i = 0; i < numToAdd; ++i)
                                                   vec.push_back (r.nextInt());
                                           }
                                       }

                                      #if LOG_RPALLOCATIONS
                                       std::cout << "num ints: " << vec.size() << "\n";
                                      #endif
                                   });
            
            using namespace std::literals;
            std::this_thread::sleep_for (1s);
            shouldExit = true;
            
            for (auto& thread : pool)
                thread.join();
            
            expect (true);
        }
    }
};

static AllocationTests allocationTests;

#endif

}} // namespace tracktion_engine
