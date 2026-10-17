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
        for (auto readAhead : { WaveNodeRealTime::ReadAhead::no, WaveNodeRealTime::ReadAhead::yes })
        {
            MESSAGE (magic_enum::enum_name (mode) << (readAhead == WaveNodeRealTime::ReadAhead::yes ? std::string_view (", read-ahead") : std::string_view()));
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
                                                            mode,
                                                            TimeStretcher::ElastiqueProOptions(),
                                                            0.0f,
                                                            readAhead);

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

    /** Returns a one second file with a single full scale sample at each of the given frames. */
    static std::unique_ptr<juce::TemporaryFile> createImpulseFile (graph::test_utilities::TestSetup ts,
                                                                   const std::vector<choc::buffer::FrameCount>& impulseFrames)
    {
        using namespace tracktion::graph::test_utilities;
        auto buffer = choc::buffer::createChannelArrayBuffer (1, (choc::buffer::FrameCount) ts.sampleRate,
                                                              [&] (auto, auto frame) -> float
                                                              {
                                                                  return std::find (impulseFrames.begin(), impulseFrames.end(), frame)
                                                                           != impulseFrames.end() ? 1.0f : 0.0f;
                                                              });

        return writeToTemporaryFile<juce::WavAudioFormat> (buffer.getView(), ts.sampleRate);
    }

    /** Checks each impulse landed on exactly the timeline sample it should have, with silence either side. */
    static void expectImpulsesAt (const juce::AudioBuffer<float>& output, int clipStartSample,
                                  const std::vector<choc::buffer::FrameCount>& impulseFrames)
    {
        for (auto frame : impulseFrames)
        {
            CAPTURE (frame);
            const auto outputFrame = clipStartSample + (int) frame;
            CHECK (output.getSample (0, outputFrame) == doctest::Approx (1.0f).epsilon (0.001));

            for (int delta : { -2, -1, 1, 2 })
            {
                CAPTURE (delta);
                CHECK (std::abs (output.getSample (0, outputFrame + delta)) < 0.001f);
            }
        }
    }

    /** Checks the source lines up with the timeline to the sample, not just roughly.
        Proxy-file clips play through a WaveNode, so any resampler latency it leaves
        uncompensated shows up as a phase offset against the same file playing
        elsewhere (e.g. a Clip FX Invert failing to null against its source).
    */
    template<typename NodeType>
    static void runSampleAlignmentTests (graph::test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *Engine::getEngines()[0];

        // Impulses at the first frame and part way through. The clip starts on a block
        // boundary so the first frame checks the resampler has no history to flush
        const std::vector<choc::buffer::FrameCount> impulseFrames { 0, 1000 };
        auto impulseFile = createImpulseFile (ts, impulseFrames);
        AudioFile impulseAudioFile (engine, impulseFile->getFile());

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);
        playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

        const auto clipStartSample = (int) ts.blockSize * 10;
        const auto clipStart = TimePosition::fromSamples (clipStartSample, ts.sampleRate);

        auto node = makeNode<NodeType> (impulseAudioFile,
                                        TimeRange (clipStart, 1_td),
                                        TimeDuration(),
                                        TimeRange(),
                                        LiveClipLevel(),
                                        1.0,
                                        ChannelConfiguration::discreteChannels (impulseAudioFile.getNumChannels()),
                                        ChannelConfiguration::discreteChannels (1),
                                        processState,
                                        EditItemID(),
                                        true);

        auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 2.0);

        CAPTURE (ts.sampleRate);
        CAPTURE (ts.blockSize);
        CAPTURE (ts.randomiseBlockSizes);

        expectImpulsesAt (testContext->buffer, clipStartSample, impulseFrames);
    }

    /** The same alignment check for SpeedRampWaveNode, which plays proxy clips that have
        speed-ramp fades and resamples through its own copy of the same code.
    */
    static void runSpeedRampSampleAlignmentTests (graph::test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph::test_utilities;
        auto& engine = *Engine::getEngines()[0];

        // A block straddling the clip start has its time clamped into the clip by
        // editTimeToFileSample, so it resamples at a non-unity ratio and can't line up to
        // the sample. Only the fixed block sizes start a block exactly on the clip
        if (ts.randomiseBlockSizes)
            return;

        // The node ramps the gain up across the first block it plays, so the impulses have
        // to sit past the largest block size to survive it
        const std::vector<choc::buffer::FrameCount> impulseFrames { 2000, 3000 };
        auto impulseFile = createImpulseFile (ts, impulseFrames);
        AudioFile impulseAudioFile (engine, impulseFile->getFile());

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);
        playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

        const auto clipStartSample = (int) ts.blockSize * 10;
        const auto clipStart = TimePosition::fromSamples (clipStartSample, ts.sampleRate);
        const auto clipRange = TimeRange (clipStart, 1_td);

        // The node requires one of its ramps to be non-empty, so fade the speed out over
        // the last 200ms, well clear of the impulses
        SpeedFadeDescription desc;
        desc.inTimeRange = TimeRange (clipStart, TimeDuration());
        desc.outTimeRange = TimeRange (clipRange.getEnd() - TimeDuration::fromSeconds (0.2),
                                       TimeDuration::fromSeconds (0.2));
        desc.fadeInType = AudioFadeCurve::linear;
        desc.fadeOutType = AudioFadeCurve::linear;

        auto node = makeNode<SpeedRampWaveNode> (impulseAudioFile,
                                                 clipRange,
                                                 TimeDuration(),
                                                 TimeRange(),
                                                 LiveClipLevel(),
                                                 1.0,
                                                 ChannelConfiguration::discreteChannels (impulseAudioFile.getNumChannels()),
                                                 ChannelConfiguration::discreteChannels (1),
                                                 processState,
                                                 EditItemID(),
                                                 true,
                                                 desc);

        auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 2.0);

        CAPTURE (ts.sampleRate);
        CAPTURE (ts.blockSize);

        expectImpulsesAt (testContext->buffer, clipStartSample, impulseFrames);
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
        runSampleAlignmentTests<WaveNode> (ts);
        runSpeedRampSampleAlignmentTests (ts);
    }

    MESSAGE ("WaveNodeRealTime");

    for (auto ts : tracktion::graph::test_utilities::getTestSetups())
    {
        runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, true);
        runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, false);
        runLoopedTimelineTests<WaveNodeRealTime> ("WaveNodeRealTime", ts);
        runSampleAlignmentTests<WaveNodeRealTime> (ts);
        runDynamicOffsetTests (ts);
        runTimestretchedTests (ts);
    }
}

} // TEST_SUITE

#endif

#if ENGINE_UNIT_TESTS_WAVENODE_READAHEAD \
    && (TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND || TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH \
        || TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH || TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE)
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

// Signalsmith passes a steady tone through cleanly so the output can be checked closely
#if ENGINE_UNIT_TESTS_PLAYBACK && ENGINE_UNIT_TESTS_CLIP_LAUNCHER && TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH

namespace looped_beat_based_clip_tests
{
    constexpr double fileBpm = 120.0;

    /** Adds a clip to the first track that plays a file in beats at fileBpm, stretched in real time,
        looping part of it until clipEnd.
    */
    inline AudioClipBase& addLoopedBeatBasedClip (Edit& edit, const juce::File& file, double fileNumBeats,
                                                  BeatRange loopRange, BeatPosition clipEnd)
    {
        auto clip = insertWaveClip (*getAudioTracks (edit)[0], {}, file, { { 0_tp, 1_tp } }, DeleteExistingClips::no);
        clip->setUsesProxy (false);
        clip->setAutoTempo (true);
        clip->getLoopInfo().setNumBeats (fileNumBeats);
        clip->setTimeStretchMode (TimeStretcher::signalsmithDefault);
        clip->setLoopRangeBeats (loopRange);
        clip->setPosition ({ { 0_tp, edit.tempoSequence.toTime (clipEnd) } });

        return *clip;
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("WaveNode: a time-stretched beat-based clip loops seamlessly")
    {
        using namespace clip_launcher_test_utilities;
        using namespace looped_beat_based_clip_tests;

        // A stereo sine and cosine, so the level and phase of each frame of the output can be checked.
        // At 120bpm, 440Hz is a whole number of cycles a beat, so the 1.5 beat file loops seamlessly
        constexpr double fileNumBeats = 1.5, toneFrequency = 440.0;
        constexpr float toneLevel = 0.5f;

        auto& engine = *Engine::getEngines()[0];
        const auto numFileFrames = (choc::buffer::FrameCount) std::llround (fileNumBeats * 60.0 / fileBpm * sampleRate);
        MemoryAudioFile file (engine, choc::buffer::createChannelArrayBuffer (2, numFileFrames, [] (auto chan, auto frame)
                                                                               {
                                                                                   const auto phase = juce::MathConstants<double>::twoPi * toneFrequency * frame / sampleRate;
                                                                                   return toneLevel * (float) (chan == 0 ? std::sin (phase) : std::cos (phase));
                                                                               }));

        // An 8 beat clip looping the file every 1.5 beats
        auto createEdit = [&] (double bpm)
        {
            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            edit->tempoSequence.getTempo (0)->setBpm (bpm);
            addLoopedBeatBasedClip (*edit, file.getFile(), fileNumBeats, { 0_bp, BeatPosition::fromBeats (fileNumBeats) }, 8_bp);

            return edit;
        };

        // Checks the output keeps the tone's level and steps its phase on steadily, from a while after
        // the clip starts (the time-stretcher takes a while to start it) to its end
        auto checkToneIsContinuous = [&] (const juce::AudioBuffer<float>& output, double outputSampleRate, TimeDuration clipLength)
        {
            const auto l = output.getReadPointer (0), r = output.getReadPointer (1);
            const auto start = toSamples (TimeDuration::fromSeconds (0.15), outputSampleRate);
            const auto end = std::min (toSamples (clipLength - TimeDuration::fromSeconds (0.01), outputSampleRate), (int64_t) output.getNumSamples());
            REQUIRE (end > start);

            // The track's pan law scales the level, so compare it with the usual level
            std::vector<double> levels;

            for (auto f = start; f < end; ++f)
                levels.push_back (std::hypot ((double) l[f], (double) r[f]));

            auto sortedLevels = levels;
            std::nth_element (sortedLevels.begin(), sortedLevels.begin() + (std::ptrdiff_t) sortedLevels.size() / 2, sortedLevels.end());
            const auto usualLevel = sortedLevels[sortedLevels.size() / 2];
            CHECK (usualLevel > toneLevel * 0.5);

            const auto phaseStep = juce::MathConstants<double>::twoPi * toneFrequency / outputSampleRate;
            int64_t numBadFrames = 0;
            double firstBadSeconds = 0.0;

            for (auto f = start; f < end; ++f)
            {
                auto phaseError = std::atan2 (l[f], r[f]) - std::atan2 (l[f - 1], r[f - 1]) - phaseStep;
                phaseError -= juce::MathConstants<double>::twoPi * std::round (phaseError / juce::MathConstants<double>::twoPi);

                if (std::abs (levels[(size_t) (f - start)] - usualLevel) <= usualLevel * 0.25 && std::abs (phaseError) <= 0.4)
                    continue;

                if (numBadFrames++ == 0)
                    firstBadSeconds = (double) f / outputSampleRate;
            }

            INFO ("First bad frame at " << firstBadSeconds << "s");
            CHECK (numBadFrames == 0);
        };

        // Playing uses a ReadAheadTimeStretchReader (the TestRunner enables it) and rendering a TimeStretchReader
        for (auto bpm : { 100.0, 150.0 })
        {
            CAPTURE (bpm);

            {
                INFO ("Playing");
                test_utilities::EnginePlayer player (engine, getPlayerParams (2));
                auto edit = createEdit (bpm);
                const auto clipLength = edit->tempoSequence.toTime (8_bp) - 0_tp;

                edit->getTransport().play (false);
                const auto output = player.process (toSamples (clipLength + TimeDuration::fromSeconds (0.1), sampleRate));
                checkToneIsContinuous (output, sampleRate, clipLength);
            }

            {
                INFO ("Rendering");
                auto edit = createEdit (bpm);
                const auto clipLength = edit->tempoSequence.toTime (8_bp) - 0_tp;
                const auto render = engine::test_utilities::renderToAudioBuffer (*edit);

                REQUIRE (render.buffer.getNumChannels() == 2);
                checkToneIsContinuous (render.buffer, render.sampleRate, clipLength);
            }
        }
    }

    TEST_CASE ("WaveNode: a time-stretched beat-based clip loops the right part of its file")
    {
        using namespace clip_launcher_test_utilities;
        using namespace looped_beat_based_clip_tests;

        // A file of three 1 beat tones, so which part of it is playing can be told from the output
        constexpr std::array<double, 3> toneFrequencies { 440.0, 880.0, 1320.0 };
        const auto framesPerBeat = (choc::buffer::FrameCount) std::llround (60.0 / fileBpm * sampleRate);

        auto& engine = *Engine::getEngines()[0];
        MemoryAudioFile file (engine, choc::buffer::createChannelArrayBuffer (1, framesPerBeat * 3, [&] (auto, auto frame)
                                                                               {
                                                                                   const auto frequency = toneFrequencies[(size_t) (frame / framesPerBeat)];
                                                                                   return (float) std::sin (juce::MathConstants<double>::twoPi * frequency * frame / sampleRate);
                                                                               }));

        // A part of the loop, in beats from its start, and the tone that should play in it
        struct Part
        {
            double start, end;
            size_t tone;
        };

        // Plays a 9 beat clip looping part of the file and checks the middle of each part of each loop plays its tone
        auto checkLoop = [&] (double bpm, BeatRange loopRange, std::optional<WarpMarker> warpMarker, std::vector<Part> parts)
        {
            test_utilities::EnginePlayer player (engine, getPlayerParams (1));
            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& ts = edit->tempoSequence;
            ts.getTempo (0)->setBpm (bpm);
            auto& clip = addLoopedBeatBasedClip (*edit, file.getFile(), 3.0, loopRange, 9_bp);

            if (warpMarker)
            {
                clip.setWarpTime (true);
                clip.getWarpTimeManager().insertMarker (*warpMarker);
            }

            edit->getTransport().play (false);
            player.process (toSamples (ts.toTime (9_bp), sampleRate));
            const auto output = player.getOutput();

            const auto loopLength = loopRange.getLength().inBeats();

            for (double loopStart = 0.0; loopStart + loopLength <= 9.0; loopStart += loopLength)
            {
                for (auto part : parts)
                {
                    const auto quarter = (part.end - part.start) / 4.0;
                    const TimeRange window (ts.toTime (BeatPosition::fromBeats (loopStart + part.start + quarter)),
                                            ts.toTime (BeatPosition::fromBeats (loopStart + part.end - quarter)));

                    // The time-stretcher takes a while to start the clip
                    if (window.getStart().inSeconds() < 0.2)
                        continue;

                    CAPTURE (loopStart);
                    CAPTURE (part.start);
                    const auto magnitude = getToneMagnitude (output, window, toneFrequencies[part.tone]);
                    CHECK (magnitude > 0.25f);

                    for (size_t other = 0; other < toneFrequencies.size(); ++other)
                        if (other != part.tone)
                            CHECK (getToneMagnitude (output, window, toneFrequencies[other]) < magnitude * 0.25f);
                }
            }
        };

        for (auto bpm : { 100.0, 150.0 })
        {
            CAPTURE (bpm);

            // Positions in a loop count from its start, so a loop that doesn't start at the
            // start of the file checks the source is looped from the right place
            {
                INFO ("Without a warp map");
                checkLoop (bpm, { 1_bp, 3_bp }, {}, { { 0.0, 1.0, 1 }, { 1.0, 2.0, 2 } });
            }

            // With a warp map, the loop is in warped time. Warping the second tone's start from 0.5s
            // to 0.75s slows the first tone down to 1.5 beats and speeds the others up to 0.75 beats.
            // N.B. The WarpReader's time-stretcher is still reset where it loops, so the output
            // isn't seamless there, but the middle of each part should play its tone
            {
                INFO ("With a warp map");
                checkLoop (bpm, { 0_bp, 3_bp }, WarpMarker (TimePosition::fromSeconds (0.5), TimePosition::fromSeconds (0.75)),
                           { { 0.0, 1.5, 0 }, { 1.5, 2.25, 1 }, { 2.25, 3.0, 2 } });
            }
        }
    }
}

#endif

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS
