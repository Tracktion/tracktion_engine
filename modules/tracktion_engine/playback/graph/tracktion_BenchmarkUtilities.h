/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_EditNodeBuilder.h"
#include "../../../tracktion_core/utilities/tracktion_Benchmark.h"
#include "../../playback/graph/tracktion_TracktionEngineNode.h"
#include "../../playback/graph/tracktion_TracktionNodePlayer.h"
#include "../../playback/graph/tracktion_MultiThreadedNodePlayer.h"

namespace tracktion::inline engine {

namespace benchmark_utilities
{
    //==============================================================================
    enum class MultiThreaded            { no, yes };

    enum class LockFree                 { no, yes };

    enum class PoolMemoryAllocations    { no, yes };

    enum class ShareNodeMemory          { no, yes };

    /** Whether to process blocks back to back as fast as possible or at the
        cadence a real audio device would ask for them.
        @see test_utilities::TestProcess::processAllInRealTime
    */
    enum class RealTime                 { no, yes };

    struct BenchmarkOptions
    {
        Edit* edit = nullptr;
        juce::String editName;
        tracktion::graph::test_utilities::TestSetup testSetup;
        MultiThreaded isMultiThreaded;
        LockFree isLockFree;
        tracktion::graph::ThreadPoolStrategy poolType;
        PoolMemoryAllocations poolMemoryAllocations = PoolMemoryAllocations::no;
        ShareNodeMemory shareNodeMemory = ShareNodeMemory::no;
        RealTime isRealTime = RealTime::no;
        int numWarmUpBlocks = 32;           /**< Blocks to process before collecting the block timings. */
        double durationToRender = 0.0;      /**< In seconds, or 0 to render the whole Edit. */
        graph::RealTimeWorkgroup* realTimeWorkgroup = nullptr; /**< For the threads to join when isRealTime is set, with the lock-free player. */
    };

    inline bool usesRealTimeWorkgroup (const BenchmarkOptions& opts)
    {
        return opts.isRealTime == RealTime::yes
            && opts.isLockFree == LockFree::yes
            && opts.realTimeWorkgroup != nullptr
            && opts.realTimeWorkgroup->isValid();
    }

    inline double getDurationToRender (const BenchmarkOptions& opts)
    {
        return opts.durationToRender > 0.0 ? opts.durationToRender
                                           : opts.edit->getLength().inSeconds();
    }

    inline juce::String getDescription (const BenchmarkOptions& opts)
    {
        using namespace tracktion::graph;
        auto s = graph::test_utilities::getDescription (opts.testSetup)
                    + juce::String (opts.isMultiThreaded == MultiThreaded::yes ? ", MT" : ", ST");

        if (opts.isMultiThreaded == MultiThreaded::yes && opts.isLockFree == LockFree::yes)
            s << ", lock-free";

        if (opts.poolMemoryAllocations == PoolMemoryAllocations::yes)
            s << ", pooled-memory";

        if (opts.shareNodeMemory == ShareNodeMemory::yes)
            s << ", share-node-memory";

        if (opts.isMultiThreaded == MultiThreaded::yes)
            s << ", " + graph::test_utilities::getName (opts.poolType);

        if (opts.isRealTime == RealTime::yes)
            s << ", real-time";

        if (usesRealTimeWorkgroup (opts))
            s << ", workgroup";

        return s;
    }

    //==============================================================================
    inline std::unique_ptr<tracktion::graph::Node> createNode (Edit& edit, ProcessState& processState,
                                                              double sampleRate, int blockSize)
    {
        CreateNodeParams params { processState };
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true; // Required for audio files to be read
        return createNodeForEdit (edit, params);
    }

    /** Renders all the blocks, then prints and adds the "rendering" result. */
    template<typename NodePlayerType>
    std::shared_ptr<tracktion::graph::test_utilities::TestContext> renderAndAddResult (const juce::String& editName, const juce::String& description,
                                                                                       tracktion::graph::test_utilities::TestProcess<NodePlayerType>& testContext,
                                                                                       RealTime isRealTime)
    {
        const StopwatchTimer sw;
        auto result = isRealTime == RealTime::yes ? testContext.processAllInRealTime()
                                                  : testContext.processAll();
        const auto stats = testContext.getStatisticsAndReset();
        const auto blockTimings = testContext.getBlockTimingStatistics();

        BenchmarkList::getInstance().addResult (createBenchmarkResult (createBenchmarkDescription ("Node",
                                                                                                   (editName + (isRealTime == RealTime::yes ? ": real-time rendering" : ": rendering")).toStdString(),
                                                                                                   description.toStdString()),
                                                                       stats, blockTimings));

        std::cout << sw.getDescription() << "\n";
        std::cout << stats.toString (testContext.getPerformanceMeasurement().getName()) << "\n";
        std::cout << blockTimings.toString() << "\n";

        return result;
    }

    template<typename NodePlayerType>
    void prepareRenderAndDestroy (juce::UnitTest& ut, juce::String editName, juce::String description,
                                  tracktion::graph::test_utilities::TestProcess<NodePlayerType>& testContext,
                                  tracktion::graph::PlayHeadState& playHeadState,
                                  MultiThreaded isMultiThreaded,
                                  RealTime isRealTime = RealTime::no)
    {
        description += ", " + juce::String (testContext.getDescription());
        ut.beginTest (editName + " - preparing: " + description);

        if (isMultiThreaded == MultiThreaded::no)
            testContext.getNodePlayer().setNumThreads (0);

        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});
        ut.expect (true);

        ut.beginTest (editName + " - memory use: " + description);
        {
            const auto nodes = tracktion::graph::getNodes (testContext.getNode(), tracktion::graph::VertexOrdering::postordering);
            const auto sizeInBytes = tracktion::graph::test_utilities::getMemoryUsage (nodes);

            BenchmarkResult bmr { createBenchmarkDescription ("Node", (editName + ": memory use").toStdString(), description.toStdString()) };
            bmr.totalSeconds = static_cast<double> (sizeInBytes);
            bmr.totalCycles = static_cast<std::uint64_t> (sizeInBytes);
            BenchmarkList::getInstance().addResult (bmr);

            std::cout << "Num nodes: " << nodes.size() << "\n";
            std::cout << juce::File::descriptionOfSizeInBytes ((int64_t) sizeInBytes) << "\n";
        }
        ut.expect (true);

        ut.beginTest (editName + " - rendering: " + description);
        auto result = renderAndAddResult (editName, description, testContext, isRealTime);
        ut.expect (true);

        ut.beginTest (editName + " - destroying: " + description);
        {
            ScopedBenchmark sb (createBenchmarkDescription ("Node", (editName + ": destroying").toStdString(), description.toStdString()));
            result.reset();
        }
        ut.expect (true);
    }

    inline void renderEdit (juce::UnitTest& ut, BenchmarkOptions opts)
    {
        assert (opts.edit != nullptr);
        assert (opts.shareNodeMemory == ShareNodeMemory::no || opts.isLockFree == LockFree::yes); // Only supported in the lock-free player atm
        const auto description = getDescription (opts);

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, opts.edit->tempoSequence };

        //===
        ut.beginTest (opts.editName + " - building: " + description);
        auto sb = std::make_unique<ScopedBenchmark> (createBenchmarkDescription ("Node", (opts.editName + ": building").toStdString(), description.toStdString()));
        auto node = createNode (*opts.edit, processState, opts.testSetup.sampleRate, opts.testSetup.blockSize);
        sb.reset();
        ut.expect (node != nullptr);

        //===
        if (opts.isLockFree == LockFree::yes)
        {
            // N.B. Don't set the Node until after the pooled and shared options or they won't get picked up
            tracktion::graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::unique_ptr<Node>(),
                                                                                                                                   processState, opts.testSetup.sampleRate, opts.testSetup.blockSize,
                                                                                                                                   tracktion::graph::getPoolCreatorFunction (opts.poolType),
                                                                                                                                   usesRealTimeWorkgroup (opts) ? opts.realTimeWorkgroup->getAudioWorkgroup()
                                                                                                                                                                : juce::AudioWorkgroup()),
                                                                                           opts.testSetup, 2, getDurationToRender (opts), false);

            testContext.setNumWarmUpBlocks (opts.numWarmUpBlocks);

            if (usesRealTimeWorkgroup (opts))
                testContext.setRealTimeWorkgroup (opts.realTimeWorkgroup);

            if (opts.poolMemoryAllocations == PoolMemoryAllocations::yes)
                testContext.getNodePlayer().enablePooledMemoryAllocations (true);

            if (opts.shareNodeMemory == ShareNodeMemory::yes)
                testContext.getNodePlayer().enableNodeMemorySharing (true);

            {
                const ScopedBenchmark sb2 (createBenchmarkDescription ("Node", (opts.editName + ": setting node").toStdString(), description.toStdString()));
                testContext.setNode(std::move(node));
            }

            prepareRenderAndDestroy (ut, opts.editName, description, testContext, playHeadState, opts.isMultiThreaded, opts.isRealTime);
        }
        else
        {
            tracktion::graph::test_utilities::TestProcess<MultiThreadedNodePlayer> testContext (std::make_unique<engine::MultiThreadedNodePlayer> (std::move (node), processState, opts.testSetup.sampleRate, opts.testSetup.blockSize),
                                                                                                opts.testSetup, 2, getDurationToRender (opts), false);
            testContext.setNumWarmUpBlocks (opts.numWarmUpBlocks);
            prepareRenderAndDestroy (ut, opts.editName, description, testContext, playHeadState, opts.isMultiThreaded, opts.isRealTime);
        }

        ut.beginTest (opts.editName + " - cleanup: " + description);
        // This is deliberately empty as RAII will take care of cleanup
        ut.expect (true);
    }

    inline void renderEdit (juce::UnitTest& ut,
                            juce::String editName,
                            Edit& edit,
                            tracktion::graph::test_utilities::TestSetup ts,
                            MultiThreaded isMultiThreaded,
                            LockFree isLockFree,
                            tracktion::graph::ThreadPoolStrategy poolType)
    {
        renderEdit (ut, { &edit, editName, ts, isMultiThreaded, isLockFree, poolType });
    }

   #ifdef DOCTEST_LIBRARY_INCLUDED
    template<typename NodePlayerType>
    void prepareRenderAndDestroy (juce::String editName, juce::String description,
                                  tracktion::graph::test_utilities::TestProcess<NodePlayerType>& testContext,
                                  tracktion::graph::PlayHeadState& playHeadState,
                                  MultiThreaded isMultiThreaded,
                                  RealTime isRealTime = RealTime::no)
    {
        description += ", " + juce::String (testContext.getDescription());

        // preparing
        if (isMultiThreaded == MultiThreaded::no)
            testContext.getNodePlayer().setNumThreads (0);

        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});

        // memory use
        {
            const auto nodes = tracktion::graph::getNodes (testContext.getNode(), tracktion::graph::VertexOrdering::postordering);
            const auto sizeInBytes = tracktion::graph::test_utilities::getMemoryUsage (nodes);

            BenchmarkResult bmr { createBenchmarkDescription ("Node", (editName + ": memory use").toStdString(), description.toStdString()) };
            bmr.totalSeconds = static_cast<double> (sizeInBytes);
            bmr.totalCycles = static_cast<std::uint64_t> (sizeInBytes);
            BenchmarkList::getInstance().addResult (bmr);

            std::cout << "Num nodes: " << nodes.size() << "\n";
            std::cout << juce::File::descriptionOfSizeInBytes ((int64_t) sizeInBytes) << "\n";
        }

        // rendering
        auto result = renderAndAddResult (editName, description, testContext, isRealTime);

        // destroying
        {
            ScopedBenchmark sb (createBenchmarkDescription ("Node", (editName + ": destroying").toStdString(), description.toStdString()));
            result.reset();
        }
    }

    inline void renderEdit (BenchmarkOptions opts)
    {
        assert (opts.edit != nullptr);
        assert (opts.shareNodeMemory == ShareNodeMemory::no || opts.isLockFree == LockFree::yes);
        const auto description = getDescription (opts);

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, opts.edit->tempoSequence };

        // building
        auto sb = std::make_unique<ScopedBenchmark> (createBenchmarkDescription ("Node", (opts.editName + ": building").toStdString(), description.toStdString()));
        auto node = createNode (*opts.edit, processState, opts.testSetup.sampleRate, opts.testSetup.blockSize);
        sb.reset();
        CHECK (node != nullptr);

        if (opts.isLockFree == LockFree::yes)
        {
            tracktion::graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::unique_ptr<Node>(),
                                                                                                                                   processState, opts.testSetup.sampleRate, opts.testSetup.blockSize,
                                                                                                                                   tracktion::graph::getPoolCreatorFunction (opts.poolType),
                                                                                                                                   usesRealTimeWorkgroup (opts) ? opts.realTimeWorkgroup->getAudioWorkgroup()
                                                                                                                                                                : juce::AudioWorkgroup()),
                                                                                           opts.testSetup, 2, getDurationToRender (opts), false);

            testContext.setNumWarmUpBlocks (opts.numWarmUpBlocks);

            if (usesRealTimeWorkgroup (opts))
                testContext.setRealTimeWorkgroup (opts.realTimeWorkgroup);

            if (opts.poolMemoryAllocations == PoolMemoryAllocations::yes)
                testContext.getNodePlayer().enablePooledMemoryAllocations (true);

            if (opts.shareNodeMemory == ShareNodeMemory::yes)
                testContext.getNodePlayer().enableNodeMemorySharing (true);

            {
                const ScopedBenchmark sb2 (createBenchmarkDescription ("Node", (opts.editName + ": setting node").toStdString(), description.toStdString()));
                testContext.setNode(std::move(node));
            }

            prepareRenderAndDestroy (opts.editName, description, testContext, playHeadState, opts.isMultiThreaded, opts.isRealTime);
        }
        else
        {
            tracktion::graph::test_utilities::TestProcess<MultiThreadedNodePlayer> testContext (std::make_unique<engine::MultiThreadedNodePlayer> (std::move (node), processState, opts.testSetup.sampleRate, opts.testSetup.blockSize),
                                                                                                opts.testSetup, 2, getDurationToRender (opts), false);
            testContext.setNumWarmUpBlocks (opts.numWarmUpBlocks);
            prepareRenderAndDestroy (opts.editName, description, testContext, playHeadState, opts.isMultiThreaded, opts.isRealTime);
        }
    }
   #endif // DOCTEST_LIBRARY_INCLUDED

    //==============================================================================
    inline std::unique_ptr<Edit> openEditfromArchiveData (Engine& engine, const char* data, int size)
    {
        std::unique_ptr<Edit> edit;
        juce::TemporaryFile tempArchiveFile, tempDir;

        {
            const auto res = tempDir.getFile().createDirectory();
            jassert (res);
            ignoreUnused (res);
        }

        {
            tempArchiveFile.getFile().replaceWithData (data, (size_t) size);
            legacy::TracktionArchiveFile archive (engine, tempArchiveFile.getFile());

            juce::Array<juce::File> createdFiles;
            const auto res = archive.extractAll (tempDir.getFile(), createdFiles);
            jassert (res);
            juce::ignoreUnused (res);

            for (const auto& f : createdFiles)
            {
                if (isTracktionEditFile (f))
                {
                    edit = loadEditFromFile (engine, f);
                    break;
                }
            }
        }

        tempDir.getFile().deleteRecursively();

        return edit;
    }

    /** Loads an Edit from a value tree with no Project file references. */
    inline std::unique_ptr<Edit> loadEditFromValueTree (Engine& engine, const juce::ValueTree& editState)
    {
        auto id = ProjectItemID::fromProperty (editState, IDs::projectID);

        if (! id.isValid())
            id = ProjectItemID::createNewID (ProjectID{});

        return Edit::createEdit (Edit::Options
        {
            engine,
            editState,
            id,
            Edit::forEditing,
            nullptr,
            Edit::getDefaultNumUndoLevels(),
            {},
            {},
            0
        });
    }

    /** Loads an Edit that was saved directly from the state to a GZip stream. */
    inline std::unique_ptr<Edit> openEditFromZipData (Engine& engine, const void* data, size_t numBytes)
    {
        return loadEditFromValueTree (engine, juce::ValueTree::readFromGZIPData (data, numBytes));
    }
}

} // namespace tracktion::inline engine
