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


namespace comp_manager_test
{
    /** Writes a sine wave of the given length to a file, creating its folder. */
    inline juce::File writeSinFile (const juce::File& dest, double seconds)
    {
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, seconds);
        dest.getParentDirectory().createDirectory();
        return sinFile->getFile().copyFileTo (dest) ? dest : juce::File();
    }

    /** Loads an Edit as the app does, identified by its owner project's item for it. */
    inline std::unique_ptr<Edit> loadEditThrough (Project& owner, const juce::File& editFile)
    {
        auto editItem = owner.getProjectItemForFile (editFile);

        if (editItem == nullptr)
            return {};

        auto itemRef = editItem->getProjectItemRef();
        return Edit::createEdit ({ owner.engine, loadEditFromFile (owner.engine, editFile, itemRef), itemRef, Edit::forExamining });
    }

    inline WaveAudioClip* getFirstWaveClip (Edit& edit)
    {
        for (auto track : getAudioTracks (edit))
            for (auto clip : track->getClips())
                if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    return waveClip;

        return nullptr;
    }

    /** Saves an Edit with one clip whose takes are the given files, in order. */
    inline bool saveEditWithTakes (Edit& edit, const juce::Array<juce::File>& takeFiles,
                                   const juce::Array<ProjectItem::Ptr>& takeItems = {})
    {
        edit.ensureNumberOfAudioTracks (1);

        // The Edit file has to exist for take paths to be made relative to its folder
        if (! test_utilities::saveEditSync (edit))
            return false;

        auto clip = insertWaveClip (*getAudioTracks (edit)[0], "Takes", takeFiles[0],
                                    { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                    DeleteExistingClips::no);

        if (clip == nullptr)
            return false;

        for (auto& f : takeFiles)
            clip->addTake (f);

        for (auto& item : takeItems)
            clip->addTake (item->getProjectItemRef());

        return test_utilities::saveEditSync (edit);
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("WaveAudioClip: setCurrentTake switches between file-based takes in an Edit without a Project")
    {
        auto& engine = *Engine::getEngines()[0];

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();
        auto editFile = tempDir.getChildFile ("Session.tracktionedit");

        auto sinFile1 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        auto sinFile2 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile1 != nullptr);
        REQUIRE (sinFile2 != nullptr);
        auto take1 = tempDir.getChildFile ("take1.wav");
        auto take2 = tempDir.getChildFile ("take2.wav");
        REQUIRE (sinFile1->getFile().copyFileTo (take1));
        REQUIRE (sinFile2->getFile().copyFileTo (take2));

        auto edit = createEmptyEdit (engine, editFile);   // no Project anywhere
        edit->ensureNumberOfAudioTracks (1);

        auto clip = insertWaveClip (*getAudioTracks (*edit)[0], "Rec", take1,
                                    { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);
        clip->addTake (take1);
        clip->addTake (take2);
        REQUIRE (clip->getNumTakes (false) == 2);

        clip->setCurrentTake (1);
        CHECK (clip->getNumTakes (false) == 2);            // the take must not be deleted
        CHECK (clip->getCurrentTake() == 1);
        CHECK (clip->getSourceFileReference().getFile() == take2);

        clip->setCurrentTake (0);
        CHECK (clip->getNumTakes (false) == 2);
        CHECK (clip->getCurrentTake() == 0);
        CHECK (clip->getSourceFileReference().getFile() == take1);

        clip = nullptr;
        edit.reset();
        tempDir.deleteRecursively (false);
    }

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

    TEST_CASE ("CompManager: multiple comps get unique source strings for folder-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
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

    TEST_CASE ("CompManager: comp take counts for folder-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
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

    TEST_CASE ("CompManager: initial comp has one section spanning full length for folder-based project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto context = CompManagerTestContext::create (engine, true);
        REQUIRE (context != nullptr);

        auto& cm = context->clip->getCompManager();
        auto compTree = cm.addNewComp();
        CHECK (compTree.getNumChildren() == 1);
        CHECK (compTree.getChild (0).hasType (IDs::COMPSECTION));
        CHECK (int (compTree.getChild (0).getProperty (IDs::takeIndex)) == -1);
    }

    TEST_CASE ("CompManager: take lengths come from each take's own file")
    {
        using namespace comp_manager_test;
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        // One take added by file and one by its project item, each a different length
        auto checkTakeLengths = [&] (ProjectType projectType)
        {
            auto projectDir = tempDir.getChildFile ("project");
            REQUIRE (projectDir.createDirectory());

            ProjectManager::TempProject tp (pm, projectType == ProjectType::folderBased ? projectDir
                                                                                          : projectDir.getChildFile ("project.tracktion"),
                                            true, projectType);
            REQUIRE (tp.project != nullptr);
            REQUIRE (tp.project->getDefaultDirectory() == projectDir);

            auto byFile = writeSinFile (projectDir.getChildFile ("Recorded").getChildFile ("by_file.wav"), 1.0);
            auto byItem = writeSinFile (projectDir.getChildFile ("Recorded").getChildFile ("by_item.wav"), 3.0);
            REQUIRE (byFile.existsAsFile());
            REQUIRE (byItem.existsAsFile());

            auto item = tp.project->createNewItem (byItem, ProjectItem::waveItemType(), "by_item", {},
                                                   ProjectItem::Category::recorded, false);
            REQUIRE (item != nullptr);

            auto editItem = tp.project->createNewEdit();
            REQUIRE (editItem != nullptr);
            auto editFile = editItem->getSourceFile();

            {
                auto edit = createEmptyEdit (engine, editFile);
                edit->setProjectItemRef (editItem->getProjectItemRef());
                REQUIRE (saveEditWithTakes (*edit, { byFile }, { item }));
            }

            auto edit = loadEditThrough (*tp.project, editFile);
            REQUIRE (edit != nullptr);

            auto clip = getFirstWaveClip (*edit);
            REQUIRE (clip != nullptr);

            auto& cm = clip->getCompManager();
            REQUIRE (cm.getNumTakes() == 2);
            CHECK (cm.getTakeLength (0) == doctest::Approx (1.0));
            CHECK (cm.getTakeLength (1) == doctest::Approx (3.0));
            CHECK (cm.getMaxCompLength() == doctest::Approx (3.0));
        };

        SUBCASE ("File-based project")      { checkTakeLengths (ProjectType::fileBased); }
        SUBCASE ("Folder-based project")    { checkTakeLengths (ProjectType::folderBased); }

        tempDir.deleteRecursively (false);
    }

    TEST_CASE ("CompManager: takes that aren't project items still find their files")
    {
        using namespace comp_manager_test;
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        // A take recorded outside the project folder is written as a path, and no project has an item for it
        auto checkTakeOutsideProject = [&] (ProjectType projectType)
        {
            auto projectDir = tempDir.getChildFile ("project");
            REQUIRE (projectDir.createDirectory());

            ProjectManager::TempProject tp (pm, projectType == ProjectType::folderBased ? projectDir
                                                                                          : projectDir.getChildFile ("project.tracktion"),
                                            true, projectType);
            REQUIRE (tp.project != nullptr);

            auto inside = writeSinFile (projectDir.getChildFile ("Recorded").getChildFile ("inside.wav"), 1.0);
            auto outside = writeSinFile (tempDir.getChildFile ("elsewhere").getChildFile ("outside.wav"), 2.0);
            REQUIRE (inside.existsAsFile());
            REQUIRE (outside.existsAsFile());

            auto editItem = tp.project->createNewEdit();
            REQUIRE (editItem != nullptr);
            auto editFile = editItem->getSourceFile();

            {
                auto edit = createEmptyEdit (engine, editFile);
                edit->setProjectItemRef (editItem->getProjectItemRef());
                REQUIRE (saveEditWithTakes (*edit, { inside, outside }));
            }

            // A file-based project registers the take as an item, a folder-based one can't
            if (projectType == ProjectType::folderBased)
                REQUIRE (tp.project->getProjectItemForFile (outside) == nullptr);

            auto edit = loadEditThrough (*tp.project, editFile);
            REQUIRE (edit != nullptr);

            auto clip = getFirstWaveClip (*edit);
            REQUIRE (clip != nullptr);

            auto& cm = clip->getCompManager();
            REQUIRE (cm.getNumTakes() == 2);
            CHECK (cm.getTakeLength (0) == doctest::Approx (1.0));
            CHECK (cm.getTakeLength (1) == doctest::Approx (2.0));
            CHECK (cm.getMaxCompLength() == doctest::Approx (2.0));
        };

        SUBCASE ("Take outside a file-based project")      { checkTakeOutsideProject (ProjectType::fileBased); }
        SUBCASE ("Take outside a folder-based project")    { checkTakeOutsideProject (ProjectType::folderBased); }

        SUBCASE ("Edit in a subfolder of its project")
        {
            // Outer/Inner/Inner Edit.tracktionedit uses Outer/Inner/Recorded/take_*.wav, written relative to the Edit
            auto outerFolder = tempDir.getChildFile ("Outer");
            auto innerFolder = outerFolder.getChildFile ("Inner");
            auto editFile = innerFolder.getChildFile ("Inner Edit.tracktionedit");
            auto take1 = writeSinFile (innerFolder.getChildFile ("Recorded").getChildFile ("take_1.wav"), 1.0);
            auto take2 = writeSinFile (innerFolder.getChildFile ("Recorded").getChildFile ("take_2.wav"), 2.0);
            REQUIRE (take1.existsAsFile());
            REQUIRE (take2.existsAsFile());

            {
                auto edit = createEmptyEdit (engine, editFile);
                REQUIRE (saveEditWithTakes (*edit, { take1, take2 }));

                auto takes = getFirstWaveClip (*edit)->getTakes();
                REQUIRE (takes.size() == 2);
                REQUIRE_FALSE (juce::File::isAbsolutePath (takes[0].toString()));
            }

            ProjectManager::TempProject outer (pm, outerFolder, false, ProjectType::folderBased);
            REQUIRE (outer.project != nullptr);

            auto edit = loadEditThrough (*outer.project, editFile);
            REQUIRE (edit != nullptr);

            auto clip = getFirstWaveClip (*edit);
            REQUIRE (clip != nullptr);

            auto& cm = clip->getCompManager();
            REQUIRE (cm.getNumTakes() == 2);
            CHECK (cm.getTakeLength (0) == doctest::Approx (1.0));
            CHECK (cm.getTakeLength (1) == doctest::Approx (2.0));
            CHECK (cm.getMaxCompLength() == doctest::Approx (2.0));
        }

        tempDir.deleteRecursively (false);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_COMP_MANAGER
