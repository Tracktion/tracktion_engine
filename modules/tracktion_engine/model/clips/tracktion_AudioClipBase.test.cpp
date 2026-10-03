/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIOCLIPBASE_CHANNELS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

struct AudioClipBaseChannelTestContext
{
    static std::unique_ptr<AudioClipBaseChannelTestContext> create (Engine& engine, int numSourceChannels)
    {
        auto context = std::make_unique<AudioClipBaseChannelTestContext>();

        context->edit = test_utilities::createTestEdit (engine);
        context->sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, numSourceChannels);
        context->track = getAudioTracks (*context->edit)[0];
        context->clip = insertWaveClip (*context->track, {}, context->sinFile->getFile(),
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);

        return context;
    }

    std::unique_ptr<Edit> edit;
    std::unique_ptr<juce::TemporaryFile> sinFile;
    AudioTrack::Ptr track;
    WaveAudioClip::Ptr clip;
};

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("AudioClipBase: activeChannelConfiguration defaults to all channels")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);
        REQUIRE (context->clip != nullptr);

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 2);
    }

    TEST_CASE ("AudioClipBase: set active channels to left only")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        context->clip->setActiveChannelConfiguration (ChannelConfiguration::mono (0));

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 1);
        CHECK (active[0].indexInDevice == 0);
    }

    TEST_CASE ("AudioClipBase: set active channels to right only")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        auto rightOnly = ChannelConfiguration (std::vector<ChannelIndex> { ChannelIndex (1, juce::AudioChannelSet::right) });
        context->clip->setActiveChannelConfiguration (rightOnly);

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 1);
        CHECK (active[0].indexInDevice == 1);
    }

    TEST_CASE ("AudioClipBase: empty config resets to all channels")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        // First set to mono
        context->clip->setActiveChannelConfiguration (ChannelConfiguration::mono (0));
        CHECK (context->clip->getActiveChannelConfiguration().getNumChannels() == 1);

        // Reset via empty config
        context->clip->setActiveChannelConfiguration (ChannelConfiguration());

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 2);
    }

    TEST_CASE ("AudioClipBase: out-of-range channels rejected")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        // Set to mono first so we have a known state
        context->clip->setActiveChannelConfiguration (ChannelConfiguration::mono (0));
        auto before = context->clip->getActiveChannelConfiguration();

        // Try to set an out-of-range channel (index 5 on a 2-channel file)
        auto outOfRange = ChannelConfiguration (std::vector<ChannelIndex> { ChannelIndex::createMono (5) });
        context->clip->setActiveChannelConfiguration (outOfRange);

        // All channels were invalid, intersection is empty, so setActiveChannelConfiguration
        // should not change the config (empty intersection means keep previous)
        auto after = context->clip->getActiveChannelConfiguration();
        CHECK (after == before);
    }

    TEST_CASE ("AudioClipBase: mixed valid/invalid channels keeps only valid")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        // Set config with index 0 (valid) and index 5 (invalid for 2-channel file)
        auto mixed = ChannelConfiguration (std::vector<ChannelIndex> {
            ChannelIndex::createMono (0),
            ChannelIndex::createMono (5)
        });
        context->clip->setActiveChannelConfiguration (mixed);

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 1);
        CHECK (active[0].indexInDevice == 0);
    }

    TEST_CASE ("AudioClipBase: new multichannel clip defaults to all source channels")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 5);
        REQUIRE (context->clip != nullptr);

        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active.getNumChannels() == 5);
    }

    TEST_CASE ("AudioClipBase: setting full source config clears stored string")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseChannelTestContext::create (engine, 2);

        // Set to the full source configuration
        auto sourceConfig = context->clip->getSourceChannelConfiguration();
        context->clip->setActiveChannelConfiguration (sourceConfig);

        // Should be equivalent to the source config
        auto active = context->clip->getActiveChannelConfiguration();
        CHECK (active == sourceConfig);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIOCLIPBASE_CHANNELS

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIOCLIPBASE_LOOPING

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

struct AudioClipBaseLoopTestContext
{
    static std::unique_ptr<AudioClipBaseLoopTestContext> create (Engine& engine)
    {
        auto context = std::make_unique<AudioClipBaseLoopTestContext>();

        context->edit = test_utilities::createTestEdit (engine);
        context->sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 2);
        context->track = getAudioTracks (*context->edit)[0];

        return context;
    }

    WaveAudioClip::Ptr insertClip (TimeRange time)
    {
        return insertWaveClip (*track, {}, sinFile->getFile(), { .time = time }, DeleteExistingClips::no);
    }

    std::unique_ptr<Edit> edit;
    std::unique_ptr<juce::TemporaryFile> sinFile;
    AudioTrack::Ptr track;
};

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("AudioClipBase: loop range survives toggling auto tempo")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseLoopTestContext::create (engine);
        auto clip = context->insertClip ({ 0_tp, 1_tp });
        REQUIRE (clip != nullptr);

        clip->setLoopRange ({ TimePosition::fromSeconds (0.25), TimePosition::fromSeconds (0.75) });
        REQUIRE (clip->isLooping());

        const auto bps = context->edit->tempoSequence.getBeatsPerSecondAt (0_tp);

        clip->setAutoTempo (true);
        CHECK (clip->isLooping());
        CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (0.25 * bps));
        CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (0.5 * bps));

        clip->setAutoTempo (false);
        CHECK (clip->isLooping());
        CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.25));
        CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (0.5));
    }

    TEST_CASE ("AudioClipBase: toggling auto tempo keeps the reported loop with a tempo change")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseLoopTestContext::create (engine);
        auto& ts = context->edit->tempoSequence;
        ts.insertTempo (1_bp, ts.getBeatsPerSecondAt (0_tp) * 60.0 * 2.0, 0.0f);

        auto clip = context->insertClip ({ 0_tp, 1_tp });
        REQUIRE (clip != nullptr);

        // The tempo doubles inside the loop
        clip->setLoopRange ({ TimePosition::fromSeconds (0.5), TimePosition::fromSeconds (2.5) });
        REQUIRE (clip->isLooping());

        const auto startBeats  = clip->getLoopStartBeats().inBeats();
        const auto lengthBeats = clip->getLoopLengthBeats().inBeats();

        clip->setAutoTempo (true);
        CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.5));
        CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (2.0));
        CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (startBeats));
        CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (lengthBeats));

        clip->setAutoTempo (false);
        CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.5));
        CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (2.0));
    }

    TEST_CASE ("AudioClipBase: cloning a beat-based loop into a time-based clip keeps the loop")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = AudioClipBaseLoopTestContext::create (engine);
        auto source = context->insertClip ({ 0_tp, 1_tp });
        auto dest = context->insertClip ({ 2_tp, 3_tp });
        REQUIRE (source != nullptr);
        REQUIRE (dest != nullptr);

        source->setLoopRange ({ TimePosition::fromSeconds (0.25), TimePosition::fromSeconds (0.75) });
        source->setAutoTempo (true);
        REQUIRE (source->beatBasedLooping());
        REQUIRE (! dest->getAutoTempo());

        dest->cloneFrom (source.get());
        CHECK (dest->getAutoTempo());
        CHECK (dest->isLooping());
        CHECK (dest->getLoopStartBeats().inBeats() == doctest::Approx (source->getLoopStartBeats().inBeats()));
        CHECK (dest->getLoopLengthBeats().inBeats() == doctest::Approx (source->getLoopLengthBeats().inBeats()));
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIOCLIPBASE_LOOPING
