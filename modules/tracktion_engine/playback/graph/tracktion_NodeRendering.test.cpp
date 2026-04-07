/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS
 #include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#endif

namespace tracktion::inline engine {

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_WAVENODE

using namespace tracktion::graph;

//==============================================================================
namespace node_rendering_benchmark_helpers
{
    struct EditTestContext
    {
        std::unique_ptr<Edit> edit;
        std::vector<std::unique_ptr<juce::TemporaryFile>> files;
    };

    static EditTestContext createBenchmarkEditContext (Engine& engine, int numTracks, int numFilesPerTrack, double durationOfFile, double sampleRate, juce::Random& r, bool useSingleFile)
    {
        auto edit = Edit::createSingleTrackEdit (engine);
        std::vector<std::unique_ptr<juce::TemporaryFile>> files;

        edit->ensureNumberOfAudioTracks (numTracks);

        if (useSingleFile)
            files.push_back (tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationOfFile, 2, 220.0f));

        for (auto t : getAudioTracks (*edit))
        {
            for (int i = 0; i < numFilesPerTrack; ++i)
            {
                if (! useSingleFile)
                {
                    const float frequency = (float) r.nextInt ({ 110, 880 });
                    auto file = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationOfFile, 2, frequency);
                    files.push_back (std::move (file));
                }

                auto& file = files.back();
                const auto timeRange = TimeRange (TimePosition::fromSeconds (durationOfFile) * i, TimeDuration::fromSeconds (durationOfFile));
                auto waveClip = t->insertWaveClip (file->getFile().getFileName(), file->getFile(),
                                                   {{ timeRange }}, false);
                waveClip->setGainDB (gainToDb (1.0f / static_cast<float> (numTracks)));
            }
        }

        return { std::move (edit), std::move (files) };
    }

    static void runWaveRendering (double durationInSeconds,
                                  int numTracks,
                                  int numFilesPerTrack,
                                  bool useSingleFile,
                                  benchmark_utilities::BenchmarkOptions opts)
    {
        using namespace tracktion::graph;
        using namespace tracktion::graph::test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = benchmark_utilities::getDescription (opts)
                                    + juce::String (useSingleFile ? ", single file" : ", multiple files");

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        //===
        // creation
        const double durationOfFile = durationInSeconds / numFilesPerTrack;
        auto context = createBenchmarkEditContext (engine, numTracks, numFilesPerTrack, durationOfFile, opts.testSetup.sampleRate, opts.testSetup.random, useSingleFile);
        CHECK (context.edit != nullptr);
        opts.edit = context.edit.get();

        const auto totalNumFiles = size_t (numTracks * numFilesPerTrack);
        auto filesSizeOk = useSingleFile || (context.files.size() == totalNumFiles);
        CHECK (filesSizeOk);
        CHECK (std::abs (context.edit->getLength().inSeconds() - durationInSeconds) <= 0.01);

        renderEdit (opts);
    }
} // namespace node_rendering_benchmark_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Node Benchmarks")
{
    using namespace tracktion::graph::test_utilities;
    using namespace node_rendering_benchmark_helpers;

    TestSetup ts;
    ts.sampleRate = 96000.0;
    ts.blockSize = 128;
    const double fileDuration = 20.0;

    using namespace benchmark_utilities;
    BenchmarkOptions opts;
    opts.editName = "Wave Edit";
    opts.testSetup = ts;
    opts.poolType = ThreadPoolStrategy::lightweightSemaphore;
    opts.isMultiThreaded = MultiThreaded::no;
    opts.isLockFree = LockFree::yes;
    opts.poolMemoryAllocations = PoolMemoryAllocations::no;

    bool singleFile = true;

    // Single threaded
    {
        singleFile = true;
        runWaveRendering (fileDuration, 20, 12, singleFile, opts);

        {
            const juce::ScopedValueSetter svs (opts.shareNodeMemory, ShareNodeMemory::yes);
            runWaveRendering (fileDuration, 20, 12, singleFile, opts);
        }

        singleFile = false;
        runWaveRendering (fileDuration, 20, 12, singleFile, opts);
    }

    // Multi-threaded strategies
    {
        opts.isMultiThreaded = MultiThreaded::yes;

        for (auto strategy : graph::test_utilities::getThreadPoolStrategies())
        {
            opts.poolType = strategy;

            singleFile = true;
            runWaveRendering (fileDuration, 20, 12, singleFile, opts);

            singleFile = false;
            runWaveRendering (fileDuration, 20, 12, singleFile, opts);
        }
    }

   #if TRACKTION_GRAPH_ADVANCED_PERFORMANCE_TESTS
    // Lightweight semaphore seems to have the best performance so compare this over different buffer sizes
    {
        singleFile = true;
        opts.poolType = ThreadPoolStrategy::lightweightSemaphore;
        opts.isMultiThreaded = MultiThreaded::yes;
        opts.isLockFree = LockFree::yes;

        for (int blockSize : { 128, 256, 512, 1024, 2048 })
        {
            opts.testSetup.blockSize = blockSize;
            runWaveRendering (fileDuration, 20, 12, singleFile, opts);
        }
    }
   #endif
}

} // TEST_SUITE

#endif

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_RESAMPLING

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Resampling Benchmarks")
{
    auto runResamplingRendering = [] (juce::String qualityName, ResamplingQuality quality)
    {
        constexpr double fileSampleRate = 96000.0;
        constexpr double playbackSampleRate = 44100.0;

        using namespace graph::test_utilities;

        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
        edit->ensureNumberOfAudioTracks (1);
        auto t = getAudioTracks (*edit)[0];

        const auto durationOfFile = 30s;
        auto sinFile = getSinFile<juce::WavAudioFormat> (fileSampleRate, 30.0, 2, 220.0f);
        const auto timeRange = TimeRange (0s, TimePosition (durationOfFile));
        auto waveClip = t->insertWaveClip (sinFile->getFile().getFileName(), sinFile->getFile(),
                                           {{ timeRange }}, false);
        waveClip->setUsesProxy (false);
        waveClip->setResamplingQuality (quality);

        Renderer::Statistics results;

        {
            ScopedBenchmark sb (createBenchmarkDescription ("Resampling", "WaveNode quality", "30s sin wave, 96KHz to 44.1Khz, " + qualityName.toStdString()));
            results = Renderer::measureStatistics ("Rendering resampling",
                                                   *edit, timeRange,
                                                   toBitSet ({ t }),
                                                   256, playbackSampleRate);
        }

        CHECK (std::abs (results.peak - 1.0f) <= 0.001f);
    };

    runResamplingRendering ("lagrange",     ResamplingQuality::lagrange);
    runResamplingRendering ("sincFast",     ResamplingQuality::sincFast);
    runResamplingRendering ("sincMedium",   ResamplingQuality::sincMedium);
    runResamplingRendering ("sincBest",     ResamplingQuality::sincBest);
}

} // TEST_SUITE

#endif

} // namespace tracktion::inline engine
