/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_EditNodeBuilder.h"
#include "../../../tracktion_core/utilities/tracktion_Benchmark.h"

namespace tracktion { inline namespace engine
{

namespace benchmark_utilities
{
    //==============================================================================
    enum class MultiThreaded            { no, yes };

    enum class LockFree                 { no, yes };

    enum class PoolMemoryAllocations    { no, yes };

    struct BenchmarkOptions
    {
        Edit* edit = nullptr;
        juce::String editName;
        tracktion::graph::test_utilities::TestSetup testSetup;
        MultiThreaded isMultiThreaded;
        LockFree isLockFree;
        tracktion::graph::ThreadPoolStrategy poolType;
        PoolMemoryAllocations poolMemoryAllocations = PoolMemoryAllocations::no;
    };

    inline juce::String getDescription (const BenchmarkOptions& opts)
    {
        using namespace tracktion::graph;
        auto s = test_utilities::getDescription (opts.testSetup)
                    + juce::String (opts.isMultiThreaded == MultiThreaded::yes ? ", MT" : ", ST");
        
        if (opts.isMultiThreaded == MultiThreaded::yes && opts.isLockFree == LockFree::yes)
            s << ", lock-free";

        if (opts.poolMemoryAllocations == PoolMemoryAllocations::yes)
            s << ", pooled-memory";

        if (opts.isMultiThreaded == MultiThreaded::yes)
            s << ", " + test_utilities::getName (opts.poolType);
        
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

    template<typename NodePlayerType>
    void prepareRenderAndDestroy (juce::UnitTest& ut, juce::String editName, juce::String description,
                                  tracktion::graph::test_utilities::TestProcess<NodePlayerType>& testContext,
                                  tracktion::graph::PlayHeadState& playHeadState,
                                  MultiThreaded isMultiThreaded)
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
            BenchmarkList::getInstance().addResult (bmr);

            std::cout << "Num nodes: " << nodes.size() << "\n";
            std::cout << juce::File::descriptionOfSizeInBytes ((int64_t) sizeInBytes) << "\n";
        }
        ut.expect (true);

        ut.beginTest (editName + " - rendering: " + description);
        const StopwatchTimer sw;
        auto result = testContext.processAll();
        const auto stats = testContext.getStatisticsAndReset();

        BenchmarkList::getInstance().addResult (createBenchmarkResult (createBenchmarkDescription ("Node",
                                                                                                   (editName + ": rendering").toStdString(),
                                                                                                   description.toStdString()),
                                                                       stats));

        std::cout << sw.getDescription() << "\n";
        std::cout << stats.toString() << "\n";
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
            tracktion::graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, opts.testSetup.sampleRate, opts.testSetup.blockSize,
                                                                                                                                  tracktion::graph::getPoolCreatorFunction (opts.poolType)),
                                                                                           opts.testSetup, 2, opts.edit->getLength().inSeconds(), false);
            
            if (opts.poolMemoryAllocations == PoolMemoryAllocations::yes)
                testContext.getNodePlayer().enablePooledMemoryAllocations (true);
                
            prepareRenderAndDestroy (ut, opts.editName, description, testContext, playHeadState, opts.isMultiThreaded);
        }
        else
        {
            tracktion::graph::test_utilities::TestProcess<MultiThreadedNodePlayer> testContext (std::make_unique<MultiThreadedNodePlayer> (std::move (node), processState, opts.testSetup.sampleRate, opts.testSetup.blockSize),
                                                                                                opts.testSetup, 2, opts.edit->getLength().inSeconds(), false);
            prepareRenderAndDestroy (ut, opts.editName, description, testContext, playHeadState, opts.isMultiThreaded);
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
            TracktionArchiveFile archive (engine, tempArchiveFile.getFile());
            
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
            id = ProjectItemID::createNewID (0);
        
        Edit::Options options =
        {
            engine,
            editState,
            id,
            
            Edit::forEditing,
            nullptr,
            Edit::getDefaultNumUndoLevels(),
            
            {},
            {}
        };
        
        return std::make_unique<Edit> (options);
    }

    /** Loads an Edit that was saved directly from the state to a GZip stream. */
    inline std::unique_ptr<Edit> openEditFromZipData (Engine& engine, const void* data, size_t numBytes)
    {
        return loadEditFromValueTree (engine, juce::ValueTree::readFromGZIPData (data, numBytes));
    }
}

}} // namespace tracktion { inline namespace engine
