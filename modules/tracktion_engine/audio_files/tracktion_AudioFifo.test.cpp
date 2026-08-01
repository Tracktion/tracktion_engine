/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FIFO

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

#include <thread>
#include <chrono>

namespace tracktion::inline engine
{

//==============================================================================
namespace audio_fifo_test_utils
{
    /** Fills a buffer so that sample n of every channel holds startValue + n. */
    inline void fillWithRamp (juce::AudioBuffer<float>& b, float startValue)
    {
        for (int channel = 0; channel < b.getNumChannels(); ++channel)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (channel, i, startValue + (float) i);
    }

    /** Returns true if every sample of every channel matches startValue + n. */
    inline bool matchesRamp (const juce::AudioBuffer<float>& b, float startValue,
                             int startSample, int numSamples)
    {
        for (int channel = 0; channel < b.getNumChannels(); ++channel)
            for (int i = 0; i < numSamples; ++i)
                if (b.getSample (channel, startSample + i) != startValue + (float) i)
                    return false;

        return true;
    }

    /** Spins until a condition is met, returning false if it takes longer than the timeout.
        Used so a broken fifo fails the test rather than hanging the test runner.
    */
    template<typename Fn>
    inline bool spinUntil (Fn&& isReady, std::chrono::milliseconds timeout = std::chrono::seconds (10))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (! isReady())
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;

            std::this_thread::yield();
        }

        return true;
    }
}

//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("AudioFifo: basic write and read")
    {
        using namespace audio_fifo_test_utils;

        // N.B. an AbstractFifo can only hold totalSize - 1 items
        AudioFifo fifo (2, 16);
        CHECK_EQ (fifo.getNumChannels(), 2);
        CHECK_EQ (fifo.getFreeSpace(), 15);
        CHECK_EQ (fifo.getNumReady(), 0);

        juce::AudioBuffer<float> src (2, 8);
        fillWithRamp (src, 100.0f);

        CHECK (fifo.write (src));
        CHECK_EQ (fifo.getNumReady(), 8);
        CHECK_EQ (fifo.getFreeSpace(), 7);

        juce::AudioBuffer<float> dest (2, 8);
        dest.clear();
        CHECK (fifo.read (dest, 0));
        CHECK (matchesRamp (dest, 100.0f, 0, 8));
        CHECK_EQ (fifo.getNumReady(), 0);
        CHECK_EQ (fifo.getFreeSpace(), 15);

        // Reading more than is ready should fail and consume nothing
        CHECK (fifo.write (src, 0, 4));
        CHECK (! fifo.read (dest, 0, 8));
        CHECK_EQ (fifo.getNumReady(), 4);

        // Writing more than there's space for should fail and add nothing
        fifo.reset();
        juce::AudioBuffer<float> tooBig (2, 20);
        fillWithRamp (tooBig, 0.0f);
        CHECK (! fifo.write (tooBig));
        CHECK_EQ (fifo.getNumReady(), 0);
    }

    TEST_CASE ("AudioFifo: reading and writing into part of a buffer")
    {
        using namespace audio_fifo_test_utils;

        AudioFifo fifo (1, 16);

        juce::AudioBuffer<float> src (1, 12);
        fillWithRamp (src, 0.0f);

        // Write samples 4-8 of the source, i.e. the ramp values 4, 5, 6, 7
        CHECK (fifo.write (src, 4, 4));
        CHECK_EQ (fifo.getNumReady(), 4);

        juce::AudioBuffer<float> dest (1, 8);
        dest.clear();
        CHECK (fifo.read (dest, 2, 4));

        CHECK (matchesRamp (dest, 4.0f, 2, 4));
        CHECK_EQ (dest.getSample (0, 0), 0.0f);
        CHECK_EQ (dest.getSample (0, 1), 0.0f);
        CHECK_EQ (dest.getSample (0, 6), 0.0f);
    }

    TEST_CASE ("AudioFifo: wrapping around the end of the buffer")
    {
        using namespace audio_fifo_test_utils;

        AudioFifo fifo (2, 16);

        juce::AudioBuffer<float> src (2, 12), dest (2, 12);

        // Push the read/write positions near the end of the storage
        fillWithRamp (src, 0.0f);
        CHECK (fifo.write (src));
        CHECK (fifo.read (dest, 0));

        // This write has to wrap, so it uses both of the AbstractFifo's blocks
        fillWithRamp (src, 500.0f);
        CHECK (fifo.write (src));
        CHECK_EQ (fifo.getNumReady(), 12);

        dest.clear();
        CHECK (fifo.read (dest, 0));
        CHECK (matchesRamp (dest, 500.0f, 0, 12));
    }

    TEST_CASE ("AudioFifo: channel count mismatches")
    {
        using namespace audio_fifo_test_utils;

        SUBCASE ("Source with fewer channels than the fifo clears the extra channels")
        {
            AudioFifo fifo (2, 16);

            // Dirty the fifo storage so the clear is actually observable
            juce::AudioBuffer<float> stereoSrc (2, 8);
            fillWithRamp (stereoSrc, 900.0f);
            CHECK (fifo.write (stereoSrc));

            juce::AudioBuffer<float> stereoDest (2, 8);
            CHECK (fifo.read (stereoDest, 0));

            juce::AudioBuffer<float> monoSrc (1, 8);
            fillWithRamp (monoSrc, 10.0f);
            CHECK (fifo.write (monoSrc, 0, 8));

            stereoDest.clear();
            CHECK (fifo.read (stereoDest, 0));

            for (int i = 0; i < 8; ++i)
            {
                CHECK_EQ (stereoDest.getSample (0, i), 10.0f + (float) i);
                CHECK_EQ (stereoDest.getSample (1, i), 0.0f);
            }
        }

        SUBCASE ("Destination with more channels than the fifo duplicates the last channel")
        {
            AudioFifo fifo (1, 16);

            juce::AudioBuffer<float> monoSrc (1, 8);
            fillWithRamp (monoSrc, 20.0f);
            CHECK (fifo.write (monoSrc));

            juce::AudioBuffer<float> stereoDest (2, 8);
            stereoDest.clear();
            CHECK (fifo.read (stereoDest, 0));

            for (int i = 0; i < 8; ++i)
            {
                CHECK_EQ (stereoDest.getSample (0, i), 20.0f + (float) i);
                CHECK_EQ (stereoDest.getSample (1, i), 20.0f + (float) i);
            }
        }

        SUBCASE ("Destination with fewer channels than the fifo takes the first channels")
        {
            AudioFifo fifo (2, 16);

            juce::AudioBuffer<float> stereoSrc (2, 8);
            fillWithRamp (stereoSrc, 30.0f);
            CHECK (fifo.write (stereoSrc));

            juce::AudioBuffer<float> monoDest (1, 8);
            monoDest.clear();
            CHECK (fifo.read (monoDest, 0));
            CHECK (matchesRamp (monoDest, 30.0f, 0, 8));
        }
    }

    TEST_CASE ("AudioFifo: writeSilence and ensureFreeSpace")
    {
        using namespace audio_fifo_test_utils;

        AudioFifo fifo (2, 16);

        juce::AudioBuffer<float> src (2, 8);
        fillWithRamp (src, 1.0f);
        CHECK (fifo.write (src));

        juce::AudioBuffer<float> dest (2, 8);
        CHECK (fifo.read (dest, 0));

        // Silence has to actually zero the storage, not just flag it as cleared
        CHECK (fifo.writeSilence (8));
        CHECK_EQ (fifo.getNumReady(), 8);

        dest.clear();
        fillWithRamp (dest, 77.0f);
        CHECK (fifo.read (dest, 0));

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                CHECK_EQ (dest.getSample (channel, i), 0.0f);

        // ensureFreeSpace should drop the oldest samples to make room
        fifo.reset();
        CHECK (fifo.write (src));
        CHECK_EQ (fifo.getFreeSpace(), 7);
        fifo.ensureFreeSpace (12);
        CHECK_EQ (fifo.getFreeSpace(), 12);
        CHECK_EQ (fifo.getNumReady(), 3);
    }

    TEST_CASE ("AudioFifo: readAdding")
    {
        using namespace audio_fifo_test_utils;

        AudioFifo fifo (2, 16);

        juce::AudioBuffer<float> src (2, 8);
        fillWithRamp (src, 0.0f);
        CHECK (fifo.write (src));

        juce::AudioBuffer<float> dest (2, 8);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                dest.setSample (channel, i, 1.0f);

        CHECK (fifo.readAdding (dest, 0));

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                CHECK_EQ (dest.getSample (channel, i), 1.0f + (float) i);
    }

    //==============================================================================
    // This is the case the real-world race showed up in: WaveNodeRealTime's audio
    // thread pushing frames into a ReadAheadTimeStretcher's fifo whilst the
    // stretcher's own thread pulls them back out. Any state the fifo shares
    // between the two sides - rather than just the disjoint sample regions the
    // AbstractFifo hands out - will be reported by ThreadSanitizer here.
    TEST_CASE ("AudioFifo: concurrent producer and consumer")
    {
        using namespace audio_fifo_test_utils;

        constexpr int numChannels = 2, fifoSize = 512, blockSize = 32, numBlocks = 2000;

        AudioFifo fifo (numChannels, fifoSize);

        std::atomic<bool> writeFailed { false }, producerTimedOut { false };

        std::thread producer ([&]
        {
            juce::AudioBuffer<float> src (numChannels, blockSize);

            for (int block = 0; block < numBlocks; ++block)
            {
                fillWithRamp (src, (float) (block * blockSize));

                if (! spinUntil ([&] { return fifo.getFreeSpace() >= blockSize; }))
                {
                    producerTimedOut = true;
                    return;
                }

                // Alternate the two write paths, both of which run on the audio thread in practice
                const bool ok = (block % 2) == 0 ? fifo.write (src.getArrayOfReadPointers(), blockSize)
                                                 : fifo.write (src, 0, blockSize);

                if (! ok)
                {
                    writeFailed = true;
                    return;
                }
            }
        });

        juce::AudioBuffer<float> dest (numChannels, blockSize);
        bool readFailed = false, dataMismatch = false, consumerTimedOut = false;

        for (int block = 0; block < numBlocks; ++block)
        {
            if (writeFailed || producerTimedOut)
                break;

            if (! spinUntil ([&] { return fifo.getNumReady() >= blockSize || writeFailed || producerTimedOut; }))
            {
                consumerTimedOut = true;
                break;
            }

            if (writeFailed || producerTimedOut)
                break;

            dest.clear();

            if (! fifo.read (dest, 0, blockSize))
            {
                readFailed = true;
                break;
            }

            if (! matchesRamp (dest, (float) (block * blockSize), 0, blockSize))
            {
                dataMismatch = true;
                break;
            }
        }

        producer.join();

        CHECK (! writeFailed);
        CHECK (! readFailed);
        CHECK (! dataMismatch);
        CHECK (! producerTimedOut);
        CHECK (! consumerTimedOut);
        CHECK_EQ (fifo.getNumReady(), 0);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FIFO
