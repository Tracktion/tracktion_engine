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

namespace tracktion_engine
{

namespace benchmark_utilities
{
    enum class MultiThreaded
    {
        no,
        yes
    };

    enum class LockFree
    {
        no,
        yes
    };

    inline std::unique_ptr<tracktion_graph::Node> createNode (Edit& edit, ProcessState& processState,
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
                                  tracktion_graph::test_utilities::TestProcess<NodePlayerType>& testContext,
                                  tracktion_graph::PlayHeadState& playHeadState,
                                  MultiThreaded isMultiThreaded)
    {
        ut.beginTest (editName + " - preparing: " + description);

        if (isMultiThreaded == MultiThreaded::no)
            testContext.getNodePlayer().setNumThreads (0);
        
        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});
        ut.expect (true);

        ut.beginTest (editName + " - memory use: " + description);
        const auto nodes = tracktion_graph::getNodes (testContext.getNode(), tracktion_graph::VertexOrdering::postordering);
        std::cout << "Num nodes: " << nodes.size() << "\n";
        std::cout << juce::File::descriptionOfSizeInBytes ((int64_t) tracktion_graph::test_utilities::getMemoryUsage (nodes)) << "\n";
        ut.expect (true);

        ut.beginTest (editName + " - rendering: " + description);
        const StopwatchTimer sw;
        auto result = testContext.processAll();
        std::cout << sw.getDescription() << "\n";
        ut.expect (true);

        ut.beginTest (editName + " - destroying: " + description);
        result.reset();
        ut.expect (true);
    }

    inline void renderEdit (juce::UnitTest& ut,
                            juce::String editName,
                            Edit& edit,
                            tracktion_graph::test_utilities::TestSetup ts,
                            MultiThreaded isMultiThreaded,
                            LockFree isLockFree,
                            tracktion_graph::ThreadPoolStrategy poolType)
    {
        const auto description = tracktion_graph::test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded == MultiThreaded::yes ? ", MT" : ", ST")
                                    + juce::String (isLockFree == LockFree::yes ? ", lock-free" : ", locking")
                                    + juce::String (isLockFree == LockFree::no ? "" : ", " + tracktion_graph::test_utilities::getName (poolType));

        tracktion_graph::PlayHead playHead;
        tracktion_graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        //===
        ut.beginTest (editName + " - building: " + description);
        auto node = createNode (edit, processState, ts.sampleRate, ts.blockSize);
        ut.expect (node != nullptr);

        //===
        if (isLockFree == LockFree::yes)
        {
            tracktion_graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                                  tracktion_graph::getPoolCreatorFunction (poolType)),
                                                                                           ts, 2, edit.getLength(), false);
            prepareRenderAndDestroy (ut, editName, description, testContext, playHeadState, isMultiThreaded);
        }
        else
        {
            tracktion_graph::test_utilities::TestProcess<MultiThreadedNodePlayer> testContext (std::make_unique<MultiThreadedNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize),
                                                                                               ts, 2, edit.getLength(), false);
            prepareRenderAndDestroy (ut, editName, description, testContext, playHeadState, isMultiThreaded);
        }
        
        ut.beginTest (editName + " - cleanup: " + description);
        // This is deliberately empty as RAII will take care of cleanup
        ut.expect (true);
    }

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

}
