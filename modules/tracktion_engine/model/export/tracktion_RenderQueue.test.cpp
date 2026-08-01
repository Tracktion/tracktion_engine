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

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("RenderQueue renders jobs sequentially in order")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2);
        auto tracks = getAudioTracks (*edit);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);

        for (auto t : tracks)
            insertWaveClip (*t, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                            DeleteExistingClips::no);

        auto destDir = juce::File::createTempFile ({});
        destDir.createDirectory();

        RenderQueue queue;

        for (auto& spec : createPerTrackSpecifications (*edit, {}, destDir))
            queue.addJob (*createRenderJob (*edit, spec));

        REQUIRE_EQ (queue.getJobs().size(), (size_t) 2);

        std::vector<RenderQueue::Job*> startedOrder, finishedOrder;
        std::atomic<bool> allFinished { false };

        queue.onJobStarted = [&] (auto& j)  { startedOrder.push_back (&j); };
        queue.onJobFinished = [&] (auto& j) { finishedOrder.push_back (&j); };
        queue.onFinished = [&]              { allFinished = true; };

        queue.start();
        test_utilities::runDispatchLoopUntilTrue (allFinished);

        const std::vector<RenderQueue::Job*> expectedOrder { queue.getJobs()[0].get(), queue.getJobs()[1].get() };
        CHECK_EQ (startedOrder, expectedOrder);
        CHECK_EQ (finishedOrder, expectedOrder);
        CHECK (queue.hasFinished());
        CHECK_EQ (queue.getTotalProgress(), 1.0f);

        for (auto& job : queue.getJobs())
        {
            CHECK (job->getState() == RenderQueue::Job::State::completed);
            CHECK (job->getFile().existsAsFile());

            auto buffer = test_utilities::loadFileInToBuffer (engine, job->getFile());
            REQUIRE (buffer.has_value());
            CHECK_EQ (buffer->getNumSamples(), (int) toSamples (1_td, 44100.0));
        }

        destDir.deleteRecursively();
    }

    TEST_CASE ("RenderQueue cancelling a pending job skips it")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2);
        auto tracks = getAudioTracks (*edit);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);

        for (auto t : tracks)
            insertWaveClip (*t, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                            DeleteExistingClips::no);

        auto destDir = juce::File::createTempFile ({});
        destDir.createDirectory();

        RenderQueue queue;

        for (auto& spec : createPerTrackSpecifications (*edit, {}, destDir))
            queue.addJob (*createRenderJob (*edit, spec));

        std::atomic<bool> allFinished { false };
        queue.onFinished = [&] { allFinished = true; };

        queue.start();
        queue.getJobs()[1]->cancel();
        test_utilities::runDispatchLoopUntilTrue (allFinished);

        CHECK (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[1]->getState() == RenderQueue::Job::State::cancelled);
        CHECK (queue.getJobs()[0]->getFile().existsAsFile());
        CHECK (! queue.getJobs()[1]->getFile().existsAsFile());

        destDir.deleteRecursively();
    }

    TEST_CASE ("RenderQueue cancelAll stops cleanly")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 5_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");

        RenderSpecification spec;
        spec.destination = destFile.getFile();

        RenderQueue queue;
        queue.addJob (*createRenderJob (*edit, spec));

        std::atomic<bool> allFinished { false };
        queue.onFinished = [&] { allFinished = true; };

        queue.start();
        queue.cancelAll();
        test_utilities::runDispatchLoopUntilTrue (allFinished);

        CHECK (queue.getJobs()[0]->getState() == RenderQueue::Job::State::cancelled);
        CHECK (queue.hasFinished());
        CHECK (! queue.getJobs()[0]->getParameters().destFile.existsAsFile());
    }

    TEST_CASE ("RenderQueue deleted mid-render leaves no partial file")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 5_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");

        RenderSpecification spec;
        spec.destination = destFile.getFile();

        auto queue = std::make_unique<RenderQueue>();
        auto job = queue->addJob (*createRenderJob (*edit, spec));

        queue->start();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        queue.reset();  // cancels the active job, joins its thread and cleans up

        CHECK (job->getState() == RenderQueue::Job::State::cancelled);
        CHECK (! job->getParameters().destFile.existsAsFile());
    }

    TEST_CASE ("RenderQueue accepts jobs appended while running and after finishing")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile1 (".wav"), destFile2 (".wav"), destFile3 (".wav");

        auto makeSpec = [] (const juce::File& f)
        {
            RenderSpecification spec;
            spec.destination = f;
            return spec;
        };

        RenderQueue queue;
        queue.addJob (*createRenderJob (*edit, makeSpec (destFile1.getFile())));

        std::atomic<int> finishedCount { 0 };
        std::atomic<bool> finished { false };
        queue.onFinished = [&] { ++finishedCount; finished = true; };

        // Append a second job while the first is active: one onFinished once both are done
        queue.start();
        queue.addJob (*createRenderJob (*edit, makeSpec (destFile2.getFile())));
        queue.start();

        test_utilities::runDispatchLoopUntilTrue (finished);
        CHECK_EQ (finishedCount.load(), 1);
        CHECK (queue.hasFinished());
        CHECK (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[1]->getState() == RenderQueue::Job::State::completed);

        // Append a third job to the finished queue: it runs and onFinished fires again
        finished = false;
        queue.addJob (*createRenderJob (*edit, makeSpec (destFile3.getFile())));
        queue.start();

        test_utilities::runDispatchLoopUntilTrue (finished);
        CHECK_EQ (finishedCount.load(), 2);
        REQUIRE_EQ (queue.getJobs().size(), (size_t) 3);
        CHECK (queue.getJobs()[2]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[2]->getFile().existsAsFile());
    }

    TEST_CASE ("RenderQueue accepts jobs appended from the onJobStarted callback")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile1 (".wav"), destFile2 (".wav");

        auto makeSpec = [] (const juce::File& f)
        {
            RenderSpecification spec;
            spec.destination = f;
            return spec;
        };

        RenderQueue queue;
        queue.addJob (*createRenderJob (*edit, makeSpec (destFile1.getFile())));

        std::atomic<bool> finished { false };
        queue.onFinished = [&] { finished = true; };

        // Appending from inside the callback reallocates the job vector whilst
        // startNextJob is still walking it
        queue.onJobStarted = [&, chained = false] (auto&) mutable
        {
            if (! std::exchange (chained, true))
                queue.addJob (*createRenderJob (*edit, makeSpec (destFile2.getFile())));
        };

        queue.start();
        test_utilities::runDispatchLoopUntilTrue (finished);

        REQUIRE_EQ (queue.getJobs().size(), (size_t) 2);
        CHECK (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[1]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[1]->getFile().existsAsFile());
    }

    TEST_CASE ("RenderQueue cancelling from the onJobStarted callback skips the render")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2);
        auto tracks = getAudioTracks (*edit);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);

        for (auto t : tracks)
            insertWaveClip (*t, {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                            DeleteExistingClips::no);

        auto destDir = juce::File::createTempFile ({});
        destDir.createDirectory();

        RenderQueue queue;

        for (auto& spec : createPerTrackSpecifications (*edit, {}, destDir))
            queue.addJob (*createRenderJob (*edit, spec));

        REQUIRE_EQ (queue.getJobs().size(), (size_t) 2);

        std::atomic<bool> finished { false };
        queue.onFinished = [&] { finished = true; };

        std::vector<RenderQueue::Job*> finishedOrder;
        queue.onJobFinished = [&] (auto& j) { finishedOrder.push_back (&j); };

        // Cancelling here happens before the job has a handle to cancel, so the
        // queue has to skip it rather than render it and throw the result away
        queue.onJobStarted = [&] (auto& j)
        {
            if (&j == queue.getJobs()[0].get())
                j.cancel();
        };

        queue.start();
        test_utilities::runDispatchLoopUntilTrue (finished);

        // The skipped job never reaches handleJobFinished, so it never reports as
        // finished - if it does, the render ran and its result was thrown away
        const std::vector<RenderQueue::Job*> expectedFinished { queue.getJobs()[1].get() };
        CHECK_EQ (finishedOrder, expectedFinished);

        CHECK (queue.getJobs()[0]->getState() == RenderQueue::Job::State::cancelled);
        CHECK (! queue.getJobs()[0]->getParameters().destFile.existsAsFile());
        CHECK (queue.getJobs()[1]->getState() == RenderQueue::Job::State::completed);
        CHECK (queue.getJobs()[1]->getFile().existsAsFile());
        CHECK (queue.hasFinished());

        destDir.deleteRecursively();
    }

    TEST_CASE ("RenderQueue rendering over an existing file replaces it")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        insertWaveClip (*getAudioTracks (*edit)[0], {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");

        RenderSpecification spec;
        spec.destination = destFile.getFile();

        auto renderOnce = [&]
        {
            RenderQueue queue;
            queue.addJob (*createRenderJob (*edit, spec));

            std::atomic<bool> finished { false };
            queue.onFinished = [&] { finished = true; };
            queue.start();
            test_utilities::runDispatchLoopUntilTrue (finished);

            REQUIRE (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);
            return destFile.getFile().getSize();
        };

        const auto firstSize = renderOnce();
        CHECK_GT (firstSize, (juce::int64) 0);

        // A file output stream opens at the end of an existing file, so without the
        // destination being deleted first this second render appends a whole second
        // wav to the first one
        CHECK_EQ (renderOnce(), firstSize);

        auto buffer = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        REQUIRE (buffer.has_value());
        CHECK_EQ (buffer->getNumSamples(), (int) toSamples (1_td, 44100.0));
    }

    TEST_CASE ("RenderQueue wrap remainder folds the tail onto the loop start")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        // A deterministic "tail": a 2s clip rendered over a 1s region, so the
        // second half plays during the end allowance. A non-integer number of
        // cycles per second makes any fold misalignment show up, and the
        // lowered gain keeps the folded sum out of clipping.
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0, 1, 223.4f);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 2_tp } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);
        clip->setGainDB (-12.0f);

        const auto renderRange = TimeRange (TimePosition(), TimePosition::fromSeconds (1.0));
        const auto tailAllowance = TimeDuration::fromSeconds (1.0);
        const auto regionSamples = (int) toSamples (1_td, 44100.0);

        // Reference: render the region plus tail without wrapping
        juce::TemporaryFile refFile (".wav");
        Renderer::Parameters refParams (*edit);
        refParams.destFile = refFile.getFile();
        refParams.time = renderRange;
        refParams.endAllowance = tailAllowance;
        refParams.audioFormat = engine.getAudioFileFormatManager().getWavFormat();

        std::atomic<bool> refDone { false };
        auto refHandle = EditRenderer::render (refParams, [&] (auto res) { CHECK (res); refDone = true; });
        test_utilities::runDispatchLoopUntilTrue (refDone);

        auto refBuffer = test_utilities::loadFileInToBuffer (engine, refFile.getFile());
        REQUIRE (refBuffer.has_value());
        REQUIRE (refBuffer->getNumSamples() > regionSamples);  // there must be a tail to fold

        // Wrapped render of the same region through the queue. There are no
        // tail-reporting plugins here, so give the job the same explicit
        // allowance the reference render used.
        juce::TemporaryFile wrapFile (".wav");
        RenderSpecification spec;
        spec.destination = wrapFile.getFile();
        spec.time = renderRange;
        spec.wrapRemainder = true;

        auto job = createRenderJob (*edit, spec);
        REQUIRE (job.has_value());
        job->params.endAllowance = tailAllowance;

        RenderQueue queue;
        queue.addJob (std::move (*job));
        REQUIRE_EQ (queue.getJobs().size(), (size_t) 1);

        std::atomic<bool> allFinished { false };
        queue.onFinished = [&] { allFinished = true; };
        queue.start();
        test_utilities::runDispatchLoopUntilTrue (allFinished);
        REQUIRE (queue.getJobs()[0]->getState() == RenderQueue::Job::State::completed);

        auto wrapped = test_utilities::loadFileInToBuffer (engine, wrapFile.getFile());
        REQUIRE (wrapped.has_value());

        // The wrapped file must be exactly the region length
        CHECK_EQ (wrapped->getNumSamples(), regionSamples);

        // And must equal the reference with its tail folded onto the start
        for (int chan = 0; chan < wrapped->getNumChannels(); ++chan)
        {
            float maxDiff = 0.0f;

            for (int i = 0; i < regionSamples; ++i)
            {
                float expected = refBuffer->getSample (chan, i);

                for (int tailPos = regionSamples + i; tailPos < refBuffer->getNumSamples(); tailPos += regionSamples)
                    expected += refBuffer->getSample (chan, tailPos);

                maxDiff = std::max (maxDiff, std::abs (wrapped->getSample (chan, i) - expected));
            }

            CHECK_LT (maxDiff, 0.001f);
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
