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
