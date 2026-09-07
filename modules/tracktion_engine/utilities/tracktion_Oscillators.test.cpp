/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_OSCILLATORS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

#include <atomic>
#include <set>
#include <thread>
#include <vector>

namespace tracktion { inline namespace engine
{

TEST_SUITE ("tracktion_engine")
{
    //==============================================================================
    static float getFirstSample (Oscillator& o)
    {
        juce::AudioBuffer<float> buffer (1, 1);
        buffer.clear();
        o.process (buffer, 0, 1);
        return buffer.getSample (0, 0);
    }

    static float getFirstSample (MultiVoiceOscillator& o)
    {
        juce::AudioBuffer<float> buffer (2, 1);
        buffer.clear();
        o.process (buffer, 0, 1);
        return buffer.getSample (0, 0);
    }

    template<typename OscillatorType>
    static OscillatorType createOscillator();

    template<>
    Oscillator createOscillator<Oscillator>()                     { return {}; }

    template<>
    MultiVoiceOscillator createOscillator<MultiVoiceOscillator>()  { return MultiVoiceOscillator (1); }

    template<typename OscillatorType>
    static std::set<float> getFirstSamplesAfterRepeatedStarts (int numStarts)
    {
        auto o = createOscillator<OscillatorType>();
        o.setSampleRate (44100.0);
        o.setWave (Oscillator::sine);

        std::set<float> firstSamples;

        for (int i = 0; i < numStarts; ++i)
        {
            o.start();
            firstSamples.insert (getFirstSample (o));
        }

        return firstSamples;
    }

    template<typename OscillatorType>
    static std::set<float> getFirstSamplesOfFreshInstances (int numInstances)
    {
        std::set<float> firstSamples;

        for (int i = 0; i < numInstances; ++i)
        {
            auto o = createOscillator<OscillatorType>();
            o.setSampleRate (44100.0);
            o.setWave (Oscillator::sine);
            o.start();
            firstSamples.insert (getFirstSample (o));
        }

        return firstSamples;
    }

    //==============================================================================
    TEST_CASE ("Oscillators: start() picks a random phase")
    {
        // Each start() should draw a new phase, and each instance should seed
        // its own generator so fresh instances don't all start from the same phase.
        // Instance counts are kept low as each setSampleRate() builds a fresh set
        // of bandlimited lookup tables, which is slow in Debug builds
        SUBCASE ("Oscillator")
        {
            CHECK (getFirstSamplesAfterRepeatedStarts<Oscillator> (8).size() > 1);
            CHECK (getFirstSamplesOfFreshInstances<Oscillator> (4).size() > 1);
        }

        SUBCASE ("MultiVoiceOscillator")
        {
            CHECK (getFirstSamplesAfterRepeatedStarts<MultiVoiceOscillator> (8).size() > 1);
            CHECK (getFirstSamplesOfFreshInstances<MultiVoiceOscillator> (4).size() > 1);
        }
    }

    TEST_CASE ("Oscillators: start() on separate instances from concurrent threads")
    {
        // Regression test for #400. start() used to draw its phase from a
        // function-local static juce::Random shared by every instance in the
        // process, so voices started at the same time on different audio threads
        // raced on its seed. Each thread here owns its own oscillators so nothing
        // should be shared between them. The race is silent without
        // ThreadSanitizer, which CI runs this under.
        constexpr int numThreads = 8;
        constexpr int numStarts = 1000;

        std::atomic<int> numReady { 0 };
        std::vector<int> numStartsCompleted ((size_t) numThreads, 0);
        std::vector<std::thread> threads;

        for (int t = 0; t < numThreads; ++t)
        {
            threads.emplace_back ([&, t]
            {
                MultiVoiceOscillator multi;
                Oscillator single;

                ++numReady;

                while (numReady < numThreads)
                    std::this_thread::yield();

                for (int i = 0; i < numStarts; ++i)
                {
                    multi.start();
                    single.start();
                }

                numStartsCompleted[(size_t) t] = numStarts;
            });
        }

        for (auto& t : threads)
            t.join();

        for (auto n : numStartsCompleted)
            CHECK_EQ (n, numStarts);
    }
}

}} // namespace tracktion { inline namespace engine

#endif // ENGINE_UNIT_TESTS_OSCILLATORS
