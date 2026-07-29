/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_RENDER_QUEUE

#include "../../../3rd_party/doctest/tracktion_doctest.hpp"
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("RenderSpecification JSON round-trip")
    {
        RenderSpecification spec;
        spec.tracks.add (EditItemID::fromRawID (1001));
        spec.tracks.add (EditItemID::fromRawID (1002));
        spec.time = TimeRange (TimePosition::fromSeconds (1.5), TimePosition::fromSeconds (4.25));
        spec.wrapRemainder = true;
        spec.destination = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("renders");
        spec.format = "flac";
        spec.sampleRate = 96000.0;
        spec.bitDepth = 24;
        spec.quality = 3;
        spec.channelLayout = "stereo";
        spec.normalise = true;
        spec.normaliseByRMS = true;
        spec.normaliseToLevelDb = -14.0f;
        spec.trimSilence = true;
        spec.dither = true;
        spec.realTime = true;
        spec.usePlugins = false;
        spec.useMasterPlugins = false;
        spec.metadata.set ("artist", "Test Artist");

        auto restored = RenderSpecification::fromJSON (spec.toJSON());

        CHECK_EQ (restored.tracks, spec.tracks);
        REQUIRE (restored.time.has_value());
        CHECK_EQ (restored.time->getStart(), spec.time->getStart());
        CHECK_EQ (restored.time->getEnd(), spec.time->getEnd());
        CHECK_EQ (restored.wrapRemainder, spec.wrapRemainder);
        CHECK_EQ (restored.destination, spec.destination);
        CHECK_EQ (restored.format, spec.format);
        CHECK_EQ (restored.sampleRate, spec.sampleRate);
        CHECK_EQ (restored.bitDepth, spec.bitDepth);
        CHECK_EQ (restored.quality, spec.quality);
        CHECK_EQ (restored.channelLayout, spec.channelLayout);
        CHECK_EQ (restored.normalise, spec.normalise);
        CHECK_EQ (restored.normaliseByRMS, spec.normaliseByRMS);
        CHECK_EQ (restored.normaliseToLevelDb, spec.normaliseToLevelDb);
        CHECK_EQ (restored.trimSilence, spec.trimSilence);
        CHECK_EQ (restored.dither, spec.dither);
        CHECK_EQ (restored.realTime, spec.realTime);
        CHECK_EQ (restored.usePlugins, spec.usePlugins);
        CHECK_EQ (restored.useMasterPlugins, spec.useMasterPlugins);
        CHECK_EQ (restored.metadata.getValue ("artist", ""), juce::String ("Test Artist"));

        SUBCASE ("defaults and unknown keys")
        {
            auto obj = new juce::DynamicObject();
            obj->setProperty ("format", "aiff");
            obj->setProperty ("someFutureKey", 42);

            juce::StringArray unknownKeys;
            auto parsed = RenderSpecification::fromJSON (juce::var (obj), &unknownKeys);

            CHECK_EQ (parsed.format, juce::String ("aiff"));
            CHECK_EQ (parsed.sampleRate, 44100.0);
            CHECK_EQ (parsed.bitDepth, 16);
            CHECK (! parsed.time.has_value());
            CHECK_EQ (unknownKeys, juce::StringArray { "someFutureKey" });
        }
    }

    TEST_CASE ("RenderSpecification validation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 2_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");

        RenderSpecification spec;
        spec.destination = destFile.getFile();
        CHECK (validateRenderSpecification (*edit, spec).wasOk());

        SUBCASE ("no destination")
        {
            spec.destination = juce::File();
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("destination can't be a directory")
        {
            spec.destination = destFile.getFile().getParentDirectory();
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("unknown format")
        {
            spec.format = "wibble";
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("invalid bit depth")
        {
            spec.bitDepth = 12;
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("unknown channel layout")
        {
            spec.channelLayout = "22.2";
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("unknown track ID")
        {
            spec.tracks.add (EditItemID::fromRawID (0xdeadbeef));
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }

        SUBCASE ("empty time range")
        {
            spec.time = TimeRange (TimePosition::fromSeconds (3.0), TimePosition::fromSeconds (3.0));
            CHECK (validateRenderSpecification (*edit, spec).failed());
        }
    }

    TEST_CASE ("RenderSpecification expansion")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 3);
        auto tracks = getAudioTracks (*edit);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);

        for (auto t : tracks)
            insertWaveClip (*t, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                            DeleteExistingClips::no);

        SUBCASE ("a specification produces one job mixing the chosen tracks")
        {
            juce::TemporaryFile destFile (".wav");

            RenderSpecification spec;
            spec.destination = destFile.getFile();
            spec.tracks = { tracks[0]->itemID, tracks[2]->itemID };

            auto job = createRenderJob (*edit, spec);
            REQUIRE (job.has_value());
            CHECK_EQ (job->params.destFile, destFile.getFile());
            CHECK_EQ (job->params.tracksToDo.countNumberOfSetBits(), 2);
            CHECK_EQ (job->params.time.getLength(), 1_td);
        }

        SUBCASE ("per-track specifications get one spec per track with unique files")
        {
            auto destDir = juce::File::createTempFile ({});
            destDir.createDirectory();

            // Two tracks with the same name mustn't collide on the same file
            tracks[1]->setName (tracks[0]->getName());

            auto specs = createPerTrackSpecifications (*edit, {}, destDir);
            REQUIRE_EQ (specs.size(), (size_t) 3);

            juce::StringArray paths;

            for (auto& spec : specs)
            {
                CHECK_EQ (spec.tracks.size(), 1);
                CHECK_EQ (spec.destination.getParentDirectory(), destDir);
                CHECK (validateRenderSpecification (*edit, spec).wasOk());
                paths.addIfNotAlreadyThere (spec.destination.getFullPathName());
            }

            CHECK_EQ (paths.size(), 3);
            destDir.deleteRecursively();
        }

        SUBCASE ("channel layouts map to the expected parameters")
        {
            juce::TemporaryFile destFile (".wav");

            RenderSpecification spec;
            spec.destination = destFile.getFile();

            spec.channelLayout = "mono";
            CHECK (createRenderJob (*edit, spec)->params.mustRenderInMono);

            spec.channelLayout = "stereo";
            CHECK_EQ (createRenderJob (*edit, spec)->params.channelConfig.getNumChannels(), 2);

            spec.channelLayout = "5.1";
            CHECK_EQ (createRenderJob (*edit, spec)->params.channelConfig.getNumChannels(), 6);
        }

        SUBCASE ("wrap remainder uses the plugin-reported tail")
        {
            juce::TemporaryFile destFile (".wav");

            RenderSpecification spec;
            spec.destination = destFile.getFile();
            spec.wrapRemainder = true;
            spec.trimSilence = true;

            auto job = createRenderJob (*edit, spec);
            REQUIRE (job.has_value());
            CHECK (job->params.wrapRemainder);
            CHECK (! job->params.trimSilenceAtEnds);  // forced off for wrapped renders
            CHECK_EQ (job->params.endAllowance, TimeDuration());  // no tail-reporting plugins here
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
