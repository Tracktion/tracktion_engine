/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>

namespace tracktion::inline engine
{

#if GRAPH_UNIT_TESTS_WAVENODE

//==============================================================================
namespace wavenode_test_helpers
{
    static std::shared_ptr<graph::test_utilities::TestContext> createTracktionTestContext (ProcessState& processState, std::unique_ptr<Node> node,
                                                                                          graph::test_utilities::TestSetup ts, int numChannels, double durationInSeconds)
    {
        graph::test_utilities::TestProcess<TracktionNodePlayer> testProcess (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                    getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                                      ts, numChannels, durationInSeconds, true);
        return testProcess.processAll();
    }

    template<typename NodeType>
    static void runBasicTests (juce::String /*nodeTypeName*/, graph::test_utilities::TestSetup ts, bool playSyncedToRange)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 5.0;
        auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());

        tracktion::graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (toSamples (0.08_tp, ts.sampleRate));
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        if (playSyncedToRange)
            playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
        else
            playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

        // at time 0s
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, fileLengthSeconds }, ts.sampleRate), 1.0f, 0.707f);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ fileLengthSeconds, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        // at time 0s, dragging
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            playHead.setUserIsDragging (true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            playHead.setUserIsDragging (false);

            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.4f, 0.282f);
        }

        // at time 1s - 4s
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimePosition (4.0s)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        // at time 1s - 4s, loop every 1s
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimePosition (4.0s)),
                                            TimeDuration(),
                                            TimeRange (0.0s, TimePosition (1.0s)),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }
    }

    template<typename NodeType>
    static void runLoopedTimelineTests (juce::String /*nodeTypeName*/, graph::test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 1.0;
        auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        // Loop 0s-1s
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            playHead.play ({ 0, timeToSample (1.0, ts.sampleRate) }, true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 5.0);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }

        // Loop 1s-2s
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimeDuration::fromSeconds (fileLengthSeconds) + 1.0s),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                            ChannelConfiguration::discreteChannels (1),
                                            processState,
                                            EditItemID(),
                                            true);

            playHead.setReferenceSampleRange ({ 0, ts.blockSize });
            playHead.play ({ timeToSample (1.0, ts.sampleRate), timeToSample (2.0, ts.sampleRate) }, true);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 5.0);
            expectAudioBuffer (testContext->buffer, 0, graph::timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }
    }

    static void runDynamicOffsetTests (graph::test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *Engine::getEngines()[0];

        const auto fileLength = 5_td;
        const auto fileLengthBeats = 5_bd;

        tempo::Sequence fileTempoSequence ({{ 0_bp, 60.0, 0.0f }},
                                           {{ 0_bp, 4, 4, false }},
                                           tempo::LengthOfOneBeat::dependsOnTimeSignature);

        auto squareFile = getSquareFile<juce::WavAudioFormat> (ts.sampleRate, fileLength.inSeconds());
        AudioFile sinAudioFile (engine, squareFile->getFile());

        tracktion::graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (toSamples (0.08_tp, ts.sampleRate));
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState, fileTempoSequence);
        playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

        // WaveNodeRealTime at time 0s, offset at 1s
        {
            auto node = std::make_unique<WaveNodeRealTime> (sinAudioFile,
                                                            TimeStretcher::Mode::disabled,
                                                            TimeStretcher::ElastiqueProOptions(),
                                                            BeatRange (0_bp, fileLengthBeats),
                                                            0_bd,
                                                            BeatRange(),
                                                            LiveClipLevel(),
                                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                                            ChannelConfiguration::discreteChannels (1),
                                                            processState,
                                                            EditItemID(),
                                                            true,
                                                            ResamplingQuality::lagrange,
                                                            SpeedFadeDescription(),
                                                            std::nullopt,
                                                            std::nullopt,
                                                            fileTempoSequence,
                                                            WaveNodeRealTime::SyncTempo::yes,
                                                            WaveNodeRealTime::SyncPitch::no,
                                                            std::nullopt);
            node->setDynamicOffsetBeats (fileLengthBeats);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, (fileLength * 3.0).inSeconds());

            expectAudioBuffer (testContext->buffer, 0, toSamples ({ 0s, fileLength }, ts.sampleRate), 0.0f, 0.0f);
            expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength), fileLength }, ts.sampleRate), 1.0f, 1.0f);
            expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength) + fileLength, fileLength }, ts.sampleRate), 0.0f, 0.0f);
        }

        // WaveNodeRealTime at time 0s, offset at -0.5s
        {
            auto node = std::make_unique<WaveNodeRealTime> (sinAudioFile,
                                                            TimeStretcher::Mode::disabled,
                                                            TimeStretcher::ElastiqueProOptions(),
                                                            BeatRange (0_bp, fileLengthBeats),
                                                            0_bd,
                                                            BeatRange(),
                                                            LiveClipLevel(),
                                                            ChannelConfiguration::discreteChannels (sinAudioFile.getNumChannels()),
                                                            ChannelConfiguration::discreteChannels (1),
                                                            processState,
                                                            EditItemID(),
                                                            true,
                                                            ResamplingQuality::lagrange,
                                                            SpeedFadeDescription(),
                                                            fileTempoSequence,
                                                            std::nullopt,
                                                            fileTempoSequence,
                                                            WaveNodeRealTime::SyncTempo::yes,
                                                            WaveNodeRealTime::SyncPitch::no,
                                                            std::nullopt);
            node->setDynamicOffsetBeats (-fileLengthBeats / 2.0);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, (fileLength * 3.0).inSeconds());

            expectAudioBuffer (testContext->buffer, 0, toSamples ({ 0_tp, fileLength / 2.0 }, ts.sampleRate), 1.0f, 1.0f);
            expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength / 2.0), toPosition (fileLength * 3.0) }, ts.sampleRate), 0.0f, 0.0f);
        }
    }

    static void runTimestretchedTests (graph::test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *Engine::getEngines()[0];

        const auto fileLength = 1_td;
        const auto fileLengthBeats = 1_bd;

        tempo::Sequence fileTempoSequence ({{ 0_bp, 60.0, 0.0f }},
                                           {{ 0_bp, 4, 4, false }},
                                           tempo::LengthOfOneBeat::dependsOnTimeSignature);

        auto squareFile = getSquareFile<juce::WavAudioFormat> (ts.sampleRate, fileLength.inSeconds());
        AudioFile squareAudioFile (engine, squareFile->getFile());

        tracktion::graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (toSamples (0.08_tp, ts.sampleRate));
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState, fileTempoSequence);
        playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

        if constexpr (TimeStretcher::defaultMode != TimeStretcher::soundtouchBetter)
        {
            // WaveNodeRealTime at time 1s, length 1s, time-stretch disabled
            {
                auto node = std::make_unique<WaveNodeRealTime> (squareAudioFile,
                                                                TimeRange (1_tp, fileLength),
                                                                0_td,
                                                                TimeRange(),
                                                                LiveClipLevel(),
                                                                1.0,
                                                                ChannelConfiguration::discreteChannels (squareAudioFile.getNumChannels()),
                                                                ChannelConfiguration::discreteChannels (1),
                                                                processState,
                                                                EditItemID(),
                                                                true,
                                                                ResamplingQuality::lagrange,
                                                                SpeedFadeDescription(),
                                                                std::nullopt,
                                                                TimeStretcher::Mode::disabled);

                auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, (fileLength * 3.0).inSeconds());

                auto f = writeToTemporaryFile<juce::WavAudioFormat> (toBufferView (testContext->buffer), ts.sampleRate, 0);

                expectAudioBuffer (testContext->buffer, 0, toSamples ({ 0s, fileLength }, ts.sampleRate), 0.0f, 0.0f);
                expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength), fileLength }, ts.sampleRate), 1.0f, 1.0f);
                expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength) + fileLength, fileLength }, ts.sampleRate), 0.0f, 0.0f);

                expectAudioBuffer (testContext->buffer, 0, toSamples ({ 1.9_tp, 2.0_tp }, ts.sampleRate), 1.0f, 1.0f);
            }

            // WaveNodeRealTime at time 1b, length 1b
            {
                auto node = std::make_unique<WaveNodeRealTime> (squareAudioFile,
                                                                TimeStretcher::Mode::disabled,
                                                                TimeStretcher::ElastiqueProOptions(),
                                                                BeatRange (1_bp, fileLengthBeats),
                                                                0_bd,
                                                                BeatRange(),
                                                                LiveClipLevel(),
                                                                ChannelConfiguration::discreteChannels (squareAudioFile.getNumChannels()),
                                                                ChannelConfiguration::discreteChannels (1),
                                                                processState,
                                                                EditItemID(),
                                                                true,
                                                                ResamplingQuality::lagrange,
                                                                SpeedFadeDescription(),
                                                                std::nullopt,
                                                                std::nullopt,
                                                                fileTempoSequence,
                                                                WaveNodeRealTime::SyncTempo::yes,
                                                                WaveNodeRealTime::SyncPitch::no,
                                                                std::nullopt);

                auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, (fileLength * 3.0).inSeconds());

                auto f = writeToTemporaryFile<juce::WavAudioFormat> (toBufferView (testContext->buffer), ts.sampleRate, 0);

                expectAudioBuffer (testContext->buffer, 0, toSamples ({ 0s, fileLength }, ts.sampleRate), 0.0f, 0.0f);
                expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength), fileLength }, ts.sampleRate), 1.0f, 1.0f);
                expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength) + fileLength, fileLength }, ts.sampleRate), 0.0f, 0.0f);

                expectAudioBuffer (testContext->buffer, 0, toSamples ({ 1.9_tp, 2.0_tp }, ts.sampleRate), 1.0f, 1.0f);
            }
        }

        // Test each enabled time-stretch algorithm for correct latency compensation.
        const TimeStretcher::Mode syncTestModes[] = {
           #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
            TimeStretcher::Mode::soundtouchBetter,
           #endif
           #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
            TimeStretcher::Mode::rubberbandMelodic,
           #endif
           #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
            TimeStretcher::Mode::signalsmithDefault,
           #endif
           #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
            TimeStretcher::Mode::elastiquePro,
           #endif
        };

        for (auto mode : syncTestModes)
        {
            MESSAGE (magic_enum::enum_name (mode));
            auto node = std::make_unique<WaveNodeRealTime> (squareAudioFile,
                                                            TimeRange (1_tp, fileLength),
                                                            0_td,
                                                            TimeRange(),
                                                            LiveClipLevel(),
                                                            1.0,
                                                            ChannelConfiguration::discreteChannels (squareAudioFile.getNumChannels()),
                                                            ChannelConfiguration::discreteChannels (1),
                                                            processState,
                                                            EditItemID(),
                                                            true,
                                                            ResamplingQuality::lagrange,
                                                            SpeedFadeDescription(),
                                                            std::nullopt,
                                                            mode);

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, (fileLength * 3.0).inSeconds());

            // Before clip: should be silent
            expectAudioBuffer (testContext->buffer, 0, toSamples ({ 0s, fileLength }, ts.sampleRate), 0.0f, 0.0f);

            // During clip (middle 80%): should have signal energy.
            auto clipRange = toSamples ({ 1.1_tp, 1.9_tp }, ts.sampleRate);
            juce::AudioBuffer<float> clipSection (testContext->buffer.getArrayOfWritePointers(),
                                                  testContext->buffer.getNumChannels(),
                                                  (int) clipRange.getStart(), (int) clipRange.getLength());
            float rms = clipSection.getRMSLevel (0, 0, clipSection.getNumSamples());
            CHECK (rms > 0.5f);

            // After clip: should be silent
            expectAudioBuffer (testContext->buffer, 0, toSamples ({ toPosition (fileLength) + fileLength, fileLength }, ts.sampleRate), 0.0f, 0.0f);
        }

        // Check the stretchers' latency compensation lines the source up with the timeline
        // rather than just producing some audio at roughly the right place. The source has
        // a silent second of its three so a mis-compensated stretcher moves the onsets.
        // The clip offset matters here as the compensation has to survive the reader being
        // repositioned, not just started from the beginning of the file
        {
            const auto onsetFileLength = 3_td;
            const auto numOnsetFrames = (choc::buffer::FrameCount) toSamples (toPosition (onsetFileLength), ts.sampleRate);
            const auto oneSecondOfFrames = numOnsetFrames / 3;
            auto onsetBuffer = choc::buffer::createChannelArrayBuffer (1, numOnsetFrames,
                                                                       [=] (auto, auto frame) -> float
                                                                       {
                                                                           if (frame >= oneSecondOfFrames && frame < oneSecondOfFrames * 2)
                                                                               return 0.0f;

                                                                           return (float) std::sin (juce::MathConstants<double>::twoPi * 220.0
                                                                                                     * (double) frame / ts.sampleRate);
                                                                       });
            auto onsetFile = writeToTemporaryFile<juce::WavAudioFormat> (onsetBuffer.getView(), ts.sampleRate, 0);
            AudioFile onsetAudioFile (engine, onsetFile->getFile());

            auto getRMS = [] (juce::AudioBuffer<float>& buffer, juce::Range<int64_t> range)
                          {
                              return buffer.getRMSLevel (0, (int) range.getStart(), (int) range.getLength());
                          };

            for (auto mode : syncTestModes)
            {
                for (auto readAhead : { WaveNodeRealTime::ReadAhead::no, WaveNodeRealTime::ReadAhead::yes })
                {
                    // Read-ahead doesn't currently support every algorithm, SoundTouch
                    // produces no output at all through it
                    if (readAhead == WaveNodeRealTime::ReadAhead::yes && mode == TimeStretcher::Mode::soundtouchBetter)
                        continue;

                    // Each entry is the clip's source offset and the sections of the timeline
                    // that must then be audible, the clip always starting at 1s
                    const std::pair<TimeDuration, std::array<bool, 3>> offsetsAndExpectedSections[] =
                    {
                        { 0_td, { true, false, true } },
                        { 1_td, { false, true, false } }
                    };

                    for (auto [offset, expectedSections] : offsetsAndExpectedSections)
                    {
                        CAPTURE (magic_enum::enum_name (mode));
                        CAPTURE (readAhead == WaveNodeRealTime::ReadAhead::yes);
                        CAPTURE (offset.inSeconds());

                        auto node = std::make_unique<WaveNodeRealTime> (onsetAudioFile,
                                                                        TimeRange (1_tp, onsetFileLength - offset),
                                                                        offset,
                                                                        TimeRange(),
                                                                        LiveClipLevel(),
                                                                        1.0,
                                                                        ChannelConfiguration::discreteChannels (onsetAudioFile.getNumChannels()),
                                                                        ChannelConfiguration::discreteChannels (1),
                                                                        processState,
                                                                        EditItemID(),
                                                                        true,
                                                                        ResamplingQuality::lagrange,
                                                                        SpeedFadeDescription(),
                                                                        std::nullopt,
                                                                        mode,
                                                                        TimeStretcher::ElastiqueProOptions(),
                                                                        0.0f,
                                                                        readAhead);

                        auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 5.0);

                        for (size_t section = 0; section < expectedSections.size(); ++section)
                        {
                            CAPTURE (section);
                            const auto sectionStart = 1.0 + (double) section;
                            const auto range = toSamples ({ TimePosition::fromSeconds (sectionStart + 0.05),
                                                            TimePosition::fromSeconds (sectionStart + 0.95) }, ts.sampleRate);

                            if (expectedSections[section])
                                CHECK_GT (getRMS (testContext->buffer, range), 0.5f);
                            else
                                CHECK_LT (getRMS (testContext->buffer, range), 0.05f);
                        }
                    }
                }
            }
        }
    }
} // namespace wavenode_test_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("WaveNode")
{
    using namespace wavenode_test_helpers;

    for (auto ts : tracktion::graph::test_utilities::getTestSetups())
    {
        runBasicTests<WaveNode> ("WaveNode", ts, true);
        runBasicTests<WaveNode> ("WaveNode", ts, false);
        runLoopedTimelineTests<WaveNode> ("WaveNode", ts);
    }

    MESSAGE ("WaveNodeRealTime");

    for (auto ts : tracktion::graph::test_utilities::getTestSetups())
    {
        runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, true);
        runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, false);
        runLoopedTimelineTests<WaveNodeRealTime> ("WaveNodeRealTime", ts);
        runDynamicOffsetTests (ts);
        runTimestretchedTests (ts);
    }
}

} // TEST_SUITE

#endif

// Currently only works with RubberBand
#if ENGINE_UNIT_TESTS_WAVENODE_READAHEAD && TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("Playback single audio clip using read-ahead")
    {
        auto& engine = *Engine::getEngines()[0];
        assert (engine.getEngineBehaviour().enableReadAheadForTimeStretchNodes()
            && "This test only works with this mode");
        test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                       .inputNames = {}, .outputNames = {} });

        auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto& tc = edit->getTransport();
        auto squareFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0);
        auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());

        AudioFile af (engine, squareFile->getFile());
        auto clip = insertWaveClip (*getAudioTracks (*edit)[0], {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
        clip->setUsesProxy (false);
        clip->setAutoTempo (true);

        auto testWithRatio = [&] (double ratio)
                             {
                                 const auto audioFileInfo = clip->getAudioFile().getInfo();
                                 const auto originalBPM = clip->getLoopInfo().getBpm (audioFileInfo);
                                 clip->getLoopInfo().setBpm (originalBPM * ratio, audioFileInfo);

                                 tc.play (false);

                                 test_utilities::waitForFileToBeMapped (af);

                                 player.process (static_cast<int> (af.getLengthInSamples()));
                                 auto output = player.getOutput();

                                 CHECK_EQ (output.getNumFrames(), af.getLengthInSamples());
                             };

        SUBCASE ("ratio 1.0")
        {
            testWithRatio (1.0);
        }

        SUBCASE ("ratio 2.0")
        {
            testWithRatio (2.0);
        }

        SUBCASE ("ratio 0.5")
        {
            testWithRatio (0.5);
        }
    }
}
#endif

#if ENGINE_UNIT_TESTS_WAVENODE_CHANNEL_ROUTING

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("WaveNode: channel routing through Edit")
    {
        using namespace tracktion::graph::test_utilities;

        auto& engine = *Engine::getEngines()[0];
        const double sampleRate = 44100.0;
        const int blockSize = 256;
        const auto fileLength = 1_td;

        auto renderEdit = [&] (Edit& edit, int numOutputChannels) -> std::shared_ptr<TestContext>
        {
            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, edit.tempoSequence };

            CreateNodeParams params { processState };
            params.sampleRate = sampleRate;
            params.blockSize = blockSize;
            params.forRendering = true;
            auto node = createNodeForEdit (edit, params);

            TestProcess<TracktionNodePlayer> testProcess (std::make_unique<TracktionNodePlayer> (std::move (node), processState, sampleRate, blockSize,
                                                                                                 getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                          TestSetup { sampleRate, blockSize, false, {} }, numOutputChannels, fileLength.inSeconds(), true);
            testProcess.setPlayHead (&playHeadState.playHead);
            playHeadState.playHead.playSyncedToRange ({});
            return testProcess.processAll();
        };

        auto checkChannel = [] (const juce::AudioBuffer<float>& buffer, int channel,
                                float expectedMag, float expectedRMS)
        {
            auto mag = buffer.getMagnitude (channel, 0, buffer.getNumSamples());
            auto rms = buffer.getRMSLevel (channel, 0, buffer.getNumSamples());
            CHECK (mag == doctest::Approx (expectedMag).epsilon (0.02));
            CHECK (rms == doctest::Approx (expectedRMS).epsilon (0.02));
        };

        std::unique_ptr<juce::TemporaryFile> sinFile;

        auto createEditWithClip = [&] (int numSourceChannels) -> std::unique_ptr<Edit>
        {
            sinFile = getSinFile<juce::WavAudioFormat> (sampleRate, fileLength.inSeconds(), numSourceChannels);
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];
            auto clip = insertWaveClip (*track, {}, sinFile->getFile(),
                                        { .time = { 0_tp, toPosition (fileLength) } },
                                        DeleteExistingClips::no);
            clip->setUsesProxy (false);

            return edit;
        };

        SUBCASE ("mono source, 1 output channel")
        {
            auto edit = createEditWithClip (1);
            auto result = renderEdit (*edit, 1);

            REQUIRE (result->buffer.getNumChannels() >= 1);
            checkChannel (result->buffer, 0, 1.0f, 0.707f);
        }

        SUBCASE ("stereo source, 2 output channels")
        {
            auto edit = createEditWithClip (2);
            auto result = renderEdit (*edit, 2);

            REQUIRE (result->buffer.getNumChannels() >= 2);
            checkChannel (result->buffer, 0, 1.0f, 0.707f);
            checkChannel (result->buffer, 1, 1.0f, 0.707f);
        }

        SUBCASE ("mono source, 2 output channels")
        {
            // The track's VolumeAndPanPlugin declares a minimum of 2 input channels,
            // so the audio graph widens the mono signal to stereo (duplicating ch0
            // into ch1) before it reaches the plugin chain.
            auto edit = createEditWithClip (1);
            auto result = renderEdit (*edit, 2);

            REQUIRE (result->buffer.getNumChannels() >= 2);
            checkChannel (result->buffer, 0, 1.0f, 0.707f);
            checkChannel (result->buffer, 1, 1.0f, 0.707f);
        }

        SUBCASE ("4-channel source, 4 output channels")
        {
            // All 4 channels flow through the track's plugin chain
            auto edit = createEditWithClip (4);
            auto result = renderEdit (*edit, 4);

            REQUIRE (result->buffer.getNumChannels() >= 4);
            checkChannel (result->buffer, 0, 1.0f, 0.707f);
            checkChannel (result->buffer, 1, 1.0f, 0.707f);
            checkChannel (result->buffer, 2, 1.0f, 0.707f);
            checkChannel (result->buffer, 3, 1.0f, 0.707f);
        }

        SUBCASE ("4-channel source, 2 output channels")
        {
            auto edit = createEditWithClip (4);
            auto result = renderEdit (*edit, 2);

            REQUIRE (result->buffer.getNumChannels() >= 2);
            checkChannel (result->buffer, 0, 1.0f, 0.707f);
            checkChannel (result->buffer, 1, 1.0f, 0.707f);
        }
    }
}

#endif // ENGINE_UNIT_TESTS_WAVENODE_CHANNEL_ROUTING

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS
