/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_COMP_MANAGER

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

struct CompManagerTestContext
{
    static std::unique_ptr<CompManagerTestContext> create (Engine& engine, bool folderBased)
    {
        auto context = std::make_unique<CompManagerTestContext>();
        auto& pm = engine.getProjectManager();

        context->tempDir = juce::File::createTempFile ({});
        context->tempDir.createDirectory();

        auto projectPath = folderBased
                             ? context->tempDir.getChildFile ("test_project")
                             : context->tempDir.getChildFile ("test_project.tracktion");

        auto projectType = folderBased ? ProjectType::folderBased : ProjectType::fileBased;

        context->tempProject = std::make_unique<ProjectManager::TempProject> (pm, projectPath, true, projectType);
        auto project = context->tempProject->project;

        if (project == nullptr)
            return {};

        project->save();

        auto editItem = project->createNewEdit();

        if (editItem == nullptr)
            return {};

        context->edit = createEmptyEdit (engine, editItem->getSourceFile());
        context->edit->setProjectItemRef (editItem->getProjectItemRef());
        context->edit->ensureNumberOfAudioTracks (1);

        context->sinFile1 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        context->sinFile2 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);

        auto audioTracks = getAudioTracks (*context->edit);

        if (audioTracks.isEmpty())
            return {};

        context->track = audioTracks[0];
        context->clip = insertWaveClip (*context->track, "TestWave",
                                        context->sinFile1->getFile(),
                                        { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                        DeleteExistingClips::no);

        if (context->clip == nullptr)
            return {};

        context->clip->addTake (context->sinFile2->getFile());

        return context;
    }

    ~CompManagerTestContext()
    {
        // Must release ref-counted pointers before destroying edit,
        // otherwise the clip destructor accesses a destroyed edit.
        clip = nullptr;
        track = nullptr;
        edit.reset();
        sinFile1.reset();
        sinFile2.reset();
        tempProject.reset();
        tempDir.deleteRecursively (false);
    }

    juce::File tempDir;
    std::unique_ptr<ProjectManager::TempProject> tempProject;
    std::unique_ptr<Edit> edit;
    std::unique_ptr<juce::TemporaryFile> sinFile1, sinFile2;
    AudioTrack::Ptr track;
    WaveAudioClip::Ptr clip;
};

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("CompManager: addNewComp creates valid comp for file-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);
        REQUIRE (context->clip != nullptr);

        auto& cm = context->clip->getCompManager();
        auto compTree = cm.addNewComp();
        CHECK (compTree.isValid());
        CHECK (bool (compTree.getProperty (IDs::isComp)) == true);

        auto source = compTree.getProperty (IDs::source).toString();
        CHECK (source.isNotEmpty());

        // File-based projects use ProjectItemIDs
        CHECK (ProjectItemID (source).isValid());
    }

    TEST_CASE ("CompManager: multiple comps get unique source strings")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();
        auto comp1 = cm.addNewComp();
        auto comp2 = cm.addNewComp();

        auto source1 = comp1.getProperty (IDs::source).toString();
        auto source2 = comp2.getProperty (IDs::source).toString();

        CHECK (source1.isNotEmpty());
        CHECK (source2.isNotEmpty());
        CHECK (source1 != source2);
    }

    TEST_CASE ("CompManager: comp take counts")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();

        auto numTakesBefore = cm.getNumTakes();
        CHECK (cm.getNumComps() == 0);
        CHECK (cm.getTotalNumTakes() == numTakesBefore);

        cm.addNewComp();

        CHECK (cm.getNumTakes() == numTakesBefore);
        CHECK (cm.getNumComps() == 1);
        CHECK (cm.getTotalNumTakes() == numTakesBefore + 1);
    }

    TEST_CASE ("CompManager: constructor fixes empty comp sources in folder-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
        REQUIRE (context != nullptr);

        // Manually insert a comp take with an empty source
        auto takesTree = context->clip->state.getChildWithName (IDs::TAKES);
        REQUIRE (takesTree.isValid());

        juce::ValueTree badTake (IDs::TAKE);
        badTake.setProperty (IDs::isComp, true, nullptr);
        badTake.setProperty (IDs::source, juce::String(), nullptr);
        takesTree.addChild (badTake, -1, nullptr);

        // Reconstruct the WaveCompManager — the constructor should fix the empty source
        WaveCompManager reconstructed (*context->clip);

        auto fixedSource = takesTree.getChild (takesTree.getNumChildren() - 1)
                               .getProperty (IDs::source).toString();
        CHECK (fixedSource.isNotEmpty());
    }

    TEST_CASE ("CompManager: constructor preserves valid comp sources")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
        REQUIRE (context != nullptr);

        // Manually insert a comp take with a valid relative path source
        auto takesTree = context->clip->state.getChildWithName (IDs::TAKES);
        REQUIRE (takesTree.isValid());

        juce::ValueTree compTake (IDs::TAKE);
        compTake.setProperty (IDs::isComp, true, nullptr);
        compTake.setProperty (IDs::source, "media/existing_comp.wav", nullptr);
        takesTree.addChild (compTake, -1, nullptr);

        // Reconstruct — the source should remain unchanged
        WaveCompManager reconstructed (*context->clip);

        auto preservedSource = takesTree.getChild (takesTree.getNumChildren() - 1)
                                   .getProperty (IDs::source).toString();
        CHECK (preservedSource == "media/existing_comp.wav");
    }

    TEST_CASE ("CompManager: constructor fixes empty comp sources in file-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);

        auto takesTree = context->clip->state.getChildWithName (IDs::TAKES);
        REQUIRE (takesTree.isValid());

        juce::ValueTree badTake (IDs::TAKE);
        badTake.setProperty (IDs::isComp, true, nullptr);
        badTake.setProperty (IDs::source, juce::String(), nullptr);
        takesTree.addChild (badTake, -1, nullptr);

        WaveCompManager reconstructed (*context->clip);

        auto fixedSource = takesTree.getChild (takesTree.getNumChildren() - 1)
                               .getProperty (IDs::source).toString();
        CHECK (fixedSource.isNotEmpty());
    }

    TEST_CASE ("CompManager: initial comp has one section spanning full length")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();
        auto compTree = cm.addNewComp();
        CHECK (compTree.getNumChildren() == 1);
        CHECK (compTree.getChild (0).hasType (IDs::COMPSECTION));
        CHECK (int (compTree.getChild (0).getProperty (IDs::takeIndex)) == -1);
    }

    TEST_CASE ("CompManager: getDefaultTakeFile uses stable clip itemID")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, false);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();
        auto compTree = cm.addNewComp();
        auto source = compTree.getProperty (IDs::source).toString();

        // File-based projects use ProjectItemIDs — the itemID is embedded
        // in the ID generation (not an index-based scheme)
        CHECK (source.isNotEmpty());
        CHECK (ProjectItemID (source).isValid());
    }

    TEST_CASE ("CompManager: folder-based project getCompManager basic operations")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();
        CHECK (cm.getTotalNumTakes() >= 1);
        CHECK (cm.getNumComps() == 0);
    }

    TEST_CASE ("CompManager: addNewComp creates valid comp for folder-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
        REQUIRE (context != nullptr);
        REQUIRE (context->clip != nullptr);

        auto& cm = context->clip->getCompManager();
        auto compTree = cm.addNewComp();
        CHECK (compTree.isValid());
        CHECK (bool (compTree.getProperty (IDs::isComp)) == true);

        auto source = compTree.getProperty (IDs::source).toString();
        CHECK (source.isNotEmpty());
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_COMP_MANAGER
