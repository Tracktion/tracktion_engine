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
        spec.mutedTracks.add (EditItemID::fromRawID (1003));
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
        CHECK_EQ (restored.mutedTracks, spec.mutedTracks);
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

        SUBCASE ("unknown muted track ID")
        {
            spec.mutedTracks.add (EditItemID::fromRawID (0xdeadbeef));
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

    TEST_CASE ("RenderSpecification advanced selection: muted tracks and submixes")
    {
        auto& engine = *Engine::getEngines()[0];

        auto renderSpec = [&engine] (Edit& edit, const RenderSpecification& spec) -> juce::AudioBuffer<float>
        {
            auto job = createRenderJob (edit, spec);
            REQUIRE (job.has_value());

            RenderQueue queue;
            queue.addJob (std::move (*job));

            std::atomic<bool> done { false };
            queue.onFinished = [&] { done = true; };
            queue.start();
            test_utilities::runDispatchLoopUntilTrue (done);
            REQUIRE (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);

            auto buffer = test_utilities::loadFileInToBuffer (engine, spec.destination);
            REQUIRE (buffer.has_value());
            return *buffer;
        };

        SUBCASE ("mutedTracks keep a track in the graph but silent in the output")
        {
            auto edit = test_utilities::createTestEdit (engine, 2);
            auto tracks = getAudioTracks (*edit);

            auto sin220 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 220.0f);
            auto sin330 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 330.0f);
            insertWaveClip (*tracks[0], {}, sin220->getFile(), { .time = { 0_tp, 1_tp } }, DeleteExistingClips::no);
            insertWaveClip (*tracks[1], {}, sin330->getFile(), { .time = { 0_tp, 1_tp } }, DeleteExistingClips::no);

            // N.B. distinct destination files: reading back through the
            // AudioFile cache would serve stale content for a reused path
            juce::TemporaryFile refFile (".wav"), mutedFile (".wav"), bothFile (".wav");

            RenderSpecification spec;
            spec.tracks = { tracks[0]->itemID };
            spec.destination = refFile.getFile();
            const auto reference = renderSpec (*edit, spec);

            spec.mutedTracks = { tracks[1]->itemID };
            spec.destination = mutedFile.getFile();
            const auto muted = renderSpec (*edit, spec);

            // The muted track's state must have been restored after the job
            CHECK (! tracks[1]->isMuted (false));

            spec.tracks = { tracks[0]->itemID, tracks[1]->itemID };
            spec.mutedTracks = {};
            spec.destination = bothFile.getFile();
            const auto both = renderSpec (*edit, spec);

            REQUIRE_EQ (muted.getNumSamples(), reference.getNumSamples());

            auto maxDifference = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
            {
                float diff = 0.0f;

                for (int i = 0; i < std::min (a.getNumSamples(), b.getNumSamples()); ++i)
                    diff = std::max (diff, std::abs (a.getSample (0, i) - b.getSample (0, i)));

                return diff;
            };

            // With track 2 muted the render matches track 1 alone; unmuted it doesn't
            CHECK_LT (maxDifference (muted, reference), 0.0001f);
            CHECK_GT (maxDifference (both, reference), 0.1f);
        }

        SUBCASE ("rendering a submix folder includes its children and bus plugins")
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto submix = edit->insertNewFolderTrack ({ nullptr, nullptr }, nullptr, true);
            REQUIRE (submix != nullptr);
            auto childTrack = edit->insertNewAudioTrack ({ submix.get(), nullptr }, nullptr);
            REQUIRE (childTrack != nullptr);

            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
            insertWaveClip (*childTrack, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                            DeleteExistingClips::no);

            constexpr float busGainDb = -6.0f;
            submix->getVolumePlugin()->setVolumeDb (busGainDb);

            juce::TemporaryFile childFile (".wav"), submixFile (".wav");

            // Reference: the child on its own renders without the submix's plugins
            RenderSpecification spec;
            spec.tracks = { childTrack->itemID };
            spec.destination = childFile.getFile();
            const auto childAlone = renderSpec (*edit, spec);

            spec.tracks = { submix->itemID };
            spec.destination = submixFile.getFile();
            const auto throughSubmix = renderSpec (*edit, spec);

            const auto childPeak = childAlone.getMagnitude (0, childAlone.getNumSamples());
            const auto submixPeak = throughSubmix.getMagnitude (0, throughSubmix.getNumSamples());

            REQUIRE_GT (childPeak, 0.1f);   // the reference must actually contain audio
            CHECK_EQ (submixPeak, doctest::Approx (childPeak * juce::Decibels::decibelsToGain (busGainDb))
                                    .epsilon (0.02));
        }
    }

    TEST_CASE ("RenderSpecification post-processing options shape the rendered audio")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);

        juce::TemporaryFile destFile (".wav");

        RenderSpecification spec;
        spec.destination = destFile.getFile();

        auto render = [&]() -> juce::AudioBuffer<float>
        {
            auto job = createRenderJob (*edit, spec);
            REQUIRE (job.has_value());

            RenderQueue queue;
            queue.addJob (std::move (*job));

            std::atomic<bool> done { false };
            queue.onFinished = [&] { done = true; };
            queue.start();
            test_utilities::runDispatchLoopUntilTrue (done);
            REQUIRE (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);

            auto buffer = test_utilities::loadFileInToBuffer (engine, spec.destination);
            REQUIRE (buffer.has_value());
            return *buffer;
        };

        SUBCASE ("normalise scales the peak to the target level")
        {
            auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            clip->setGainDB (-12.0f);

            spec.normalise = true;
            spec.normaliseToLevelDb = -6.0f;

            auto buffer = render();
            CHECK_EQ (buffer.getMagnitude (0, buffer.getNumSamples()),
                      doctest::Approx (juce::Decibels::decibelsToGain (-6.0f)).epsilon (0.02));
        }

        SUBCASE ("RMS adjustment scales the average level to the target")
        {
            auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            clip->setGainDB (-12.0f);

            spec.normaliseByRMS = true;
            spec.normaliseToLevelDb = -12.0f;

            auto buffer = render();
            CHECK_EQ (buffer.getRMSLevel (0, 0, buffer.getNumSamples()),
                      doctest::Approx (juce::Decibels::decibelsToGain (-12.0f)).epsilon (0.02));
        }

        SUBCASE ("trim silence removes the silent lead-in and tail")
        {
            // A 1s clip in the middle of a 3s range: only the clip should remain
            insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 1_tp, 2_tp } },
                            DeleteExistingClips::no);

            spec.time = TimeRange (TimePosition(), TimePosition::fromSeconds (3.0));
            spec.trimSilence = true;

            auto buffer = render();
            const auto seconds = buffer.getNumSamples() / 44100.0;
            CHECK_GT (seconds, 0.9);
            CHECK_LT (seconds, 1.2);
        }

        SUBCASE ("dithering adds low-level noise below the 16-bit floor")
        {
            // At -100 dB the sine sits around the 16-bit LSB, so it mostly
            // quantises to silence unless dither noise is added.
            // N.B. each render goes to its own file: the AudioFile cache would
            // serve stale content if one path were reused.
            auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            clip->setGainDB (-100.0f);

            auto countNonZeroSamples = [] (const juce::AudioBuffer<float>& b)
            {
                int count = 0;

                for (int i = 0; i < b.getNumSamples(); ++i)
                    if (b.getSample (0, i) != 0.0f)
                        ++count;

                return count;
            };

            const auto undithered = render();

            juce::TemporaryFile ditheredFile (".wav");
            spec.destination = ditheredFile.getFile();
            spec.dither = true;
            const auto dithered = render();

            // The dither noise must actually change the quantised output...
            float maxDiff = 0.0f;

            for (int i = 0; i < std::min (undithered.getNumSamples(), dithered.getNumSamples()); ++i)
                maxDiff = std::max (maxDiff, std::abs (undithered.getSample (0, i) - dithered.getSample (0, i)));

            CHECK_GT (maxDiff, 0.0f);

            // ...and push more of this sub-LSB signal above the quantisation floor
            CHECK_GT (countNonZeroSamples (dithered), countNonZeroSamples (undithered));
        }
    }

    TEST_CASE ("RenderSpecification realtime render matches the offline render")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        // Short clip: a realtime render takes as long as the rendered region
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 0.5);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, TimePosition::fromSeconds (0.5) } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);

        auto renderWith = [&] (bool realTime) -> juce::AudioBuffer<float>
        {
            // Each render goes to its own file to dodge the AudioFile cache
            juce::TemporaryFile destFile (".wav");

            RenderSpecification spec;
            spec.destination = destFile.getFile();
            spec.realTime = realTime;

            auto job = createRenderJob (*edit, spec);
            REQUIRE (job.has_value());

            RenderQueue queue;
            queue.addJob (std::move (*job));

            std::atomic<bool> done { false };
            queue.onFinished = [&] { done = true; };
            queue.start();
            test_utilities::runDispatchLoopUntilTrue (done);
            REQUIRE (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);

            auto buffer = test_utilities::loadFileInToBuffer (engine, spec.destination);
            REQUIRE (buffer.has_value());
            return *buffer;
        };

        const auto offline  = renderWith (false);
        const auto realtime = renderWith (true);

        // Not silence, same length, and sample-accurate agreement
        CHECK_GT (offline.getMagnitude (0, offline.getNumSamples()), 0.1f);
        REQUIRE_EQ (offline.getNumSamples(), realtime.getNumSamples());
        REQUIRE_EQ (offline.getNumChannels(), realtime.getNumChannels());

        float maxDiff = 0.0f;

        for (int ch = 0; ch < offline.getNumChannels(); ++ch)
            for (int i = 0; i < offline.getNumSamples(); ++i)
                maxDiff = std::max (maxDiff, std::abs (offline.getSample (ch, i) - realtime.getSample (ch, i)));

        CHECK_LT (maxDiff, 0.0001f);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
