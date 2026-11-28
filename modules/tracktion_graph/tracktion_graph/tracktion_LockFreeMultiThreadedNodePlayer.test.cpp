/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_LOCKFREENODEPLAYER

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("LockFreeMultiThreadedNodePlayer: concurrent graph swap stress")
    {
        // Repeatedly posts new graphs from a non-real-time thread whilst blocks
        // are being processed and pool threads are running. This stresses the
        // PreparedNode hand-over between the pushing thread, the audio thread
        // and the free-running pool threads, which has previously caused data
        // races where a pool thread could read a PreparedNode whilst it was
        // being swapped or destroyed. Run under TSan/ASan to catch regressions
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 64;
        constexpr int numBlocksToProcess = 2000;

        auto makeGraph = [] (int numSources)
        {
            std::vector<std::unique_ptr<Node>> nodes;

            for (int i = 0; i < numSources; ++i)
                nodes.push_back (std::make_unique<SinNode> (220.0f * (float) (i + 1)));

            return std::make_unique<BasicSummingNode> (std::move (nodes));
        };

        for (auto strategy : { ThreadPoolStrategy::realTime,
                               ThreadPoolStrategy::lightweightSemHybrid,
                               ThreadPoolStrategy::lightweightSemaphoreWakeQueued })
        {
            LockFreeMultiThreadedNodePlayer player { getPoolCreatorFunction (strategy) };
            player.setNumThreads (4);
            player.setNode (makeGraph (4), sampleRate, blockSize);

            std::atomic<bool> shouldStopPushing { false };
            std::atomic<int> numGraphsPushed { 0 };

            std::thread pushingThread ([&]
            {
                for (int i = 0; ! shouldStopPushing; ++i)
                {
                    player.setNode (makeGraph (2 + (i % 6)), sampleRate, blockSize);
                    ++numGraphsPushed;

                    if ((i % 8) == 0)
                        std::this_thread::yield();
                }
            });

            choc::buffer::ChannelArrayBuffer<float> audioBuffer;
            audioBuffer.resize ({ (choc::buffer::ChannelCount) 1, (choc::buffer::FrameCount) blockSize });
            tracktion_engine::MidiMessageArray midi;
            int64_t numSamplesDone = 0;

            auto processBlock = [&]
            {
                audioBuffer.clear();
                midi.clear();
                const auto referenceSampleRange = juce::Range<int64_t>::withStartAndLength (numSamplesDone, blockSize);
                player.process ({ (choc::buffer::FrameCount) blockSize, referenceSampleRange, { audioBuffer.getView(), midi } });
                numSamplesDone += blockSize;
            };

            for (int block = 0; block < numBlocksToProcess; ++block)
            {
                processBlock();

                // Give the pushing thread a chance to get in between blocks
                if ((block % 16) == 0)
                    std::this_thread::yield();
            }

            shouldStopPushing = true;
            pushingThread.join();

            CHECK (numGraphsPushed.load() > 0);

            // Check the player still produces output after all the swapping
            player.setNode (makeGraph (4), sampleRate, blockSize);

            float maxMagnitude = 0.0f;

            for (int block = 0; block < 10; ++block)
            {
                processBlock();
                auto juceBuffer = toAudioBuffer (audioBuffer.getView());
                maxMagnitude = std::max (maxMagnitude, juceBuffer.getMagnitude (0, 0, juceBuffer.getNumSamples()));
            }

            CHECK (maxMagnitude > 0.0f);

            player.clearNode();
        }
    }

    TEST_CASE ("LockFreeMultiThreadedNodePlayer: threads with no Nodes to process come back to the block")
    {
        // One slow Node feeds several chains, so the pool threads have nothing
        // to do for most of each block and the chains are only queued once the
        // slow Node has finished. There is a gap between blocks as well, so
        // strategies that put threads to sleep have to wake them at the start
        // of a block and when the chains are queued during it. The number of
        // threads is changed part way through as that recreates them
        using namespace std::chrono_literals;
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 64;
        constexpr int numBlocksToProcess = 150;
        constexpr int numChains = 8;

        for (auto strategy : test_utilities::getThreadPoolStrategies())
        {
            CAPTURE (test_utilities::getName (strategy).toStdString());

            auto slowNode = std::make_shared<FunctionNode> (makeNode<SinNode> (220.0f),
                                                            [numSamples = 0] (float s) mutable
                                                            {
                                                                if ((numSamples++ % blockSize) == 0)
                                                                    std::this_thread::sleep_for (200us);

                                                                return s;
                                                            });
            std::vector<std::unique_ptr<Node>> chains;

            for (int i = 0; i < numChains; ++i)
                chains.push_back (makeGainNode (makeNode<ForwardingNode> (slowNode), 1.0f / numChains));

            LockFreeMultiThreadedNodePlayer player { getPoolCreatorFunction (strategy) };
            player.setNumThreads (4);
            player.setNode (std::make_unique<BasicSummingNode> (std::move (chains)), sampleRate, blockSize);

            choc::buffer::ChannelArrayBuffer<float> audioBuffer;
            audioBuffer.resize ({ (choc::buffer::ChannelCount) 1, (choc::buffer::FrameCount) blockSize });
            tracktion_engine::MidiMessageArray midi;
            juce::AudioBuffer<float> output (1, numBlocksToProcess * blockSize);
            int64_t numSamplesDone = 0;

            for (int block = 0; block < numBlocksToProcess; ++block)
            {
                if (block == numBlocksToProcess / 3)
                    player.setNumThreads (2);
                else if (block == 2 * numBlocksToProcess / 3)
                    player.setNumThreads (4);

                audioBuffer.clear();
                midi.clear();
                const auto referenceSampleRange = juce::Range<int64_t>::withStartAndLength (numSamplesDone, blockSize);
                player.process ({ (choc::buffer::FrameCount) blockSize, referenceSampleRange, { audioBuffer.getView(), midi } });

                output.copyFrom (0, (int) numSamplesDone, toAudioBuffer (audioBuffer.getView()), 0, 0, blockSize);
                numSamplesDone += blockSize;

                std::this_thread::sleep_for (200us);
            }

            // The chains sum back to the sin wave if they were all processed in every block
            CHECK (output.getMagnitude (0, 0, output.getNumSamples()) == doctest::Approx (1.0f).epsilon (0.01));
            CHECK (output.getRMSLevel (0, 0, output.getNumSamples()) == doctest::Approx (0.707f).epsilon (0.01));

            player.clearNode();
        }
    }
}

} // namespace tracktion::inline graph

#endif
