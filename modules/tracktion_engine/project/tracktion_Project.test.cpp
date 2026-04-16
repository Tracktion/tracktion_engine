/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PROJECT

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../utilities/tracktion_TestUtilities.h"
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Project: create with edits and clips")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // Use a temp directory for the whole test
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a temp project
        auto projectFile = tempDir.getChildFile ("test_project.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;

        REQUIRE (project != nullptr);
        CHECK (project->isValid());
        CHECK (project->save());

        // Create a sin wave file to use as audio source
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);
        REQUIRE (sinFile->getFile().existsAsFile());

        // Create 2 edits
        auto editItem1 = project->createNewEdit();
        auto editItem2 = project->createNewEdit();
        REQUIRE (editItem1 != nullptr);
        REQUIRE (editItem2 != nullptr);

        // -- Edit 1: one audio clip + one MIDI clip --
        {
            auto edit = createEmptyEdit (engine, editItem1->getSourceFile());
            edit->setProjectItemRef (editItem1->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            // Audio clip on track 0
            auto* audioTrack = audioTracks[0];
            auto waveClip = insertWaveClip (*audioTrack, "TestWave",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            // MIDI clip on track 1
            auto* midiTrack = audioTracks[1];
            auto midiClip = insertMIDIClip (*midiTrack, "TestMIDI",
                                            { 0_tp, TimePosition::fromSeconds (4.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (1.0), 100, 0, nullptr);
            midiClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                             BeatDuration::fromBeats (0.5), 80, 0, nullptr);
            midiClip->getSequence().addNote (67, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (2.0), 127, 0, nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        // -- Edit 2: one audio clip --
        {
            auto edit = createEmptyEdit (engine, editItem2->getSourceFile());
            edit->setProjectItemRef (editItem2->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (1);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 1);

            auto waveClip = insertWaveClip (*audioTracks[0], "TestWave2",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        // Verify project has items (2 edits + audio file items added by insertWaveClip)
        auto numItems = project->getNumProjectItems();
        CHECK (numItems >= 2); // At least the 2 edits

        // Verify edit items exist
        CHECK (project->getProjectItemFor (editItem1->getProjectItemRef()) != nullptr);
        CHECK (project->getProjectItemFor (editItem2->getProjectItemRef()) != nullptr);

        // Verify the audio file was added as a project item
        auto audioItem = project->getProjectItemForFile (sinFile->getFile());
        CHECK (audioItem != nullptr);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: basic operations")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // Create a temp folder to act as the project root
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("test_folder_project");
        projectFolder.createDirectory();

        // Drop a .wav file into the folder
        auto wavFile = projectFolder.getChildFile ("audio.wav");
        wavFile.create();
        REQUIRE (wavFile.existsAsFile());

        // Drop a .tracktionedit file into the folder
        auto editFile = projectFolder.getChildFile ("my_edit.tracktionedit");
        editFile.create();
        REQUIRE (editFile.existsAsFile());

        // Add a file in a "Recorded" subfolder
        auto recordedDir = projectFolder.getChildFile ("Recorded");
        recordedDir.createDirectory();
        auto recordedWav = recordedDir.getChildFile ("take1.wav");
        recordedWav.create();

        // Create the folder-based project via TempProject
        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;

        REQUIRE (project != nullptr);

        // isValid — folder exists
        CHECK (project->isValid());

        // getName — folder name
        CHECK (project->getName() == "test_folder_project");

        // getProjectID — folder-based uses hash of folder path
        CHECK (project->getProjectID().isValid());

        // save — always returns true
        CHECK (project->save());

        // getSelectableDescription
        CHECK (project->getSelectableDescription() == TRANS("Folder Project"));

        // getDefaultDirectory — returns the folder itself
        CHECK (project->getDefaultDirectory() == projectFolder);

        // getAllProjectItems — should discover all 3 files
        auto items = project->getAllProjectItems();
        CHECK (items.size() == 3);

        // getProjectItemForFile
        auto wavItem = project->getProjectItemForFile (wavFile);
        REQUIRE (wavItem != nullptr);
        CHECK (wavItem->getType() == ProjectItem::waveItemType());
        CHECK (wavItem->getName() == "audio");
        CHECK (wavItem->getSourceFile() == wavFile);

        auto editItem = project->getProjectItemForFile (editFile);
        REQUIRE (editItem != nullptr);
        CHECK (editItem->getType() == ProjectItem::editItemType());

        // hasBeenDeleted — should be false for folder items
        CHECK_FALSE (wavItem->hasBeenDeleted());
        CHECK_FALSE (editItem->hasBeenDeleted());

        // getProject() should return the owning project
        CHECK (wavItem->getProject() == project);
        CHECK (editItem->getProject() == project);

        // ProjectItemID should be invalid for folder-backed items
        CHECK_FALSE (wavItem->getProjectItemRef().isProjectItemID());
        CHECK_FALSE (editItem->getProjectItemRef().isProjectItemID());

        // Category: edit in root should be Category::edit
        CHECK (editItem->getCategory() == ProjectItem::Category::edit);

        // Verify category inference for "Recorded" subfolder
        auto recordedItem = project->getProjectItemForFile (recordedWav);
        REQUIRE (recordedItem != nullptr);
        CHECK (recordedItem->getCategory() == ProjectItem::Category::recorded);

        // getAllProjectItemRefs — should return refs for all 3 discovered items
        auto allRefs = project->getAllProjectItemRefs();
        CHECK (allRefs.size() == 3);

        for (auto& ref : allRefs)
        {
            CHECK (ref.isValid());
            CHECK_FALSE (ref.isProjectItemID());
        }

        // getIndexOf — should find each item by its ref
        for (int i = 0; i < project->getNumProjectItems(); ++i)
        {
            auto ref = project->getProjectItemRef (i);
            CHECK (project->getIndexOf (ref) == i);
        }

        // getIndexOf — unknown ref should return -1
        CHECK (project->getIndexOf (ProjectItemRef::fromPath ("nonexistent.wav")) == -1);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: create with edits and clips")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // Use a temp directory for the whole test
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a folder-based project
        auto projectFolder = tempDir.getChildFile ("test_folder_project");
        projectFolder.createDirectory();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;

        REQUIRE (project != nullptr);
        CHECK (project->isValid());

        // Create a sin wave file to use as audio source
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);
        REQUIRE (sinFile->getFile().existsAsFile());

        // Create 2 edits
        auto editItem1 = project->createNewEdit();
        auto editItem2 = project->createNewEdit();
        REQUIRE (editItem1 != nullptr);
        REQUIRE (editItem2 != nullptr);

        auto editSourceFile1 = editItem1->getSourceFile();
        auto editSourceFile2 = editItem2->getSourceFile();

        // -- Edit 1: one audio clip + one MIDI clip --
        {
            auto edit = createEmptyEdit (engine, editSourceFile1);
            edit->setProjectItemRef (editItem1->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            // Audio clip on track 0
            auto waveClip = insertWaveClip (*audioTracks[0], "TestWave",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            // File reference should resolve to the original file
            REQUIRE (! waveClip->getSourceFileReference().isUsingProjectReference());
            REQUIRE (waveClip->getSourceFileReference().getFile() == sinFile->getFile());

            // MIDI clip on track 1
            auto midiClip = insertMIDIClip (*audioTracks[1], "TestMIDI",
                                            { 0_tp, TimePosition::fromSeconds (4.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (1.0), 100, 0, nullptr);
            midiClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                             BeatDuration::fromBeats (0.5), 80, 0, nullptr);
            midiClip->getSequence().addNote (67, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (2.0), 127, 0, nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        // -- Edit 2: one audio clip --
        {
            auto edit = createEmptyEdit (engine, editSourceFile2);
            edit->setProjectItemRef (editItem2->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (1);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 1);

            auto waveClip = insertWaveClip (*audioTracks[0], "TestWave2",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        // Verify project has items (2 edits + potentially audio items)
        CHECK (project->getNumProjectItems() >= 2);

        // Verify edit items can be found by file
        CHECK (project->getProjectItemForFile (editSourceFile1) != nullptr);
        CHECK (project->getProjectItemForFile (editSourceFile2) != nullptr);

        // Re-load edit 1 and verify content
        {
            auto loadedEdit = loadEditFromFile (engine, editSourceFile1);
            REQUIRE (loadedEdit != nullptr);

            auto audioTracks = getAudioTracks (*loadedEdit);
            CHECK (audioTracks.size() >= 2);

            // Verify MIDI clip with 3 notes
            bool foundMidiClip = false;
            for (auto* track : audioTracks)
            {
                for (auto* clip : track->getClips())
                {
                    if (auto* midiClip = dynamic_cast<MidiClip*> (clip))
                    {
                        foundMidiClip = true;
                        CHECK (midiClip->getSequence().getNotes().size() == 3);
                    }
                }
            }
            CHECK (foundMidiClip);

            // Verify audio clip exists
            bool foundAudioClip = false;
            for (auto* track : audioTracks)
            {
                for (auto* clip : track->getClips())
                {
                    if (dynamic_cast<WaveAudioClip*> (clip) != nullptr)
                    {
                        foundAudioClip = true;
                        break;
                    }
                }
            }
            CHECK (foundAudioClip);
        }

        cleanup();
    }

    TEST_CASE ("Project: archive and unarchive round-trip")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // === Setup: Create a project with content ===
        auto projectFile = tempDir.getChildFile ("roundtrip_project.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        REQUIRE (project->isValid());

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        // Create an edit with audio + MIDI
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        int originalNoteCount = 0;

        {
            auto edit = createEmptyEdit (engine, editItem->getSourceFile());
            edit->setProjectItemRef (editItem->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            // Audio clip
            auto waveClip = insertWaveClip (*audioTracks[0], "RoundTripWave",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            // MIDI clip
            auto midiClip = insertMIDIClip (*audioTracks[1], "RoundTripMIDI",
                                            { 0_tp, TimePosition::fromSeconds (4.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (1.0), 100, 0, nullptr);
            midiClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                             BeatDuration::fromBeats (0.5), 80, 0, nullptr);
            midiClip->getSequence().addNote (67, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (2.0), 127, 0, nullptr);
            originalNoteCount = midiClip->getSequence().getNotes().size();

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        // Record original project state
        auto originalProjectId = project->getProjectID();
        auto originalNumItems = project->getNumProjectItems();

        // Collect original item info
        struct ItemInfo
        {
            juce::String type;
            juce::String name;
        };
        std::vector<ItemInfo> originalItems;
        for (int i = 0; i < originalNumItems; ++i)
        {
            auto item = project->getProjectItemAt (i);
            if (item != nullptr)
                originalItems.push_back ({ item->getType(), item->getName() });
        }

        // === Archive ===
        auto archiveFile = tempDir.getChildFile ("test_archive.trkx");
        {
            legacy::TracktionArchiveFile archive (engine, archiveFile);

            // Add project file
            CHECK (archive.addFile (projectFile, projectFile.getParentDirectory(),
                                    legacy::TracktionArchiveFile::CompressionType::none));

            // Add all source files from project items
            for (int i = 0; i < project->getNumProjectItems(); ++i)
            {
                auto item = project->getProjectItemAt (i);
                if (item != nullptr)
                {
                    auto sourceFile = item->getSourceFile();
                    if (sourceFile.existsAsFile() && sourceFile != projectFile)
                    {
                        archive.addFile (sourceFile, projectFile.getParentDirectory(),
                                         legacy::TracktionArchiveFile::CompressionType::none);
                    }
                }
            }

            archive.flush();
        }

        // Verify archive
        {
            legacy::TracktionArchiveFile archive (engine, archiveFile);
            CHECK (archive.isValidArchive());
            CHECK (archive.getNumFiles() >= 2); // At least project file + edit file
        }

        // === Unarchive ===
        auto extractDir = tempDir.getChildFile ("extracted");
        extractDir.createDirectory();

        juce::Array<juce::File> extractedFiles;
        {
            legacy::TracktionArchiveFile archive (engine, archiveFile);
            CHECK (archive.extractAll (extractDir, extractedFiles));
        }

        CHECK (extractedFiles.size() >= 2);

        // Find the extracted project file
        juce::File extractedProjectFile;
        for (auto& f : extractedFiles)
        {
            if (f.hasFileExtension (".tracktion"))
            {
                extractedProjectFile = f;
                break;
            }
        }
        REQUIRE (extractedProjectFile.existsAsFile());

        // Create a new project from the extracted file using TempProject
        ProjectManager::TempProject extractedTp (pm, extractedProjectFile, false);
        auto newProject = extractedTp.project;
        REQUIRE (newProject != nullptr);

        // Remap IDs
        auto newProjectId = newProject->getProjectID();
        if (newProjectId != originalProjectId)
            newProject->redirectIDsFromProject (originalProjectId, newProjectId);

        newProject->createNewProjectId();
        newProject->save();

        // === Verify ===
        // Check number of items matches
        CHECK_EQ (newProject->getNumProjectItems(), originalNumItems);

        // Check item types and names
        for (int i = 0; i < newProject->getNumProjectItems() && i < (int) originalItems.size(); ++i)
        {
            auto item = newProject->getProjectItemAt (i);
            REQUIRE (item != nullptr);
            CHECK_EQ (item->getType().toStdString(), originalItems[(size_t) i].type.toStdString());
        }

        // Check source files exist on disk
        for (int i = 0; i < newProject->getNumProjectItems(); ++i)
        {
            auto item = newProject->getProjectItemAt (i);
            if (item != nullptr)
            {
                auto sourceFile = item->getSourceFile();
                if (sourceFile != juce::File())
                    CHECK_MESSAGE (sourceFile.existsAsFile(),
                                   "Source file missing: " << sourceFile.getFullPathName());
            }
        }

        // Load the edit and verify content
        {
            // Find the edit item in the new project
            ProjectItem::Ptr newEditItem;
            for (int i = 0; i < newProject->getNumProjectItems(); ++i)
            {
                auto item = newProject->getProjectItemAt (i);
                if (item != nullptr && item->getType() == ProjectItem::editItemType())
                {
                    newEditItem = item;
                    break;
                }
            }
            REQUIRE (newEditItem != nullptr);

            auto loadedEdit = loadEditForExamining (pm, newEditItem->getProjectItemRef());
            REQUIRE (loadedEdit != nullptr);

            auto audioTracks = getAudioTracks (*loadedEdit);
            CHECK (audioTracks.size() >= 2);

            // Verify MIDI content
            bool foundMidiClip = false;
            for (auto* track : audioTracks)
            {
                for (auto* clip : track->getClips())
                {
                    if (auto* midiClip = dynamic_cast<MidiClip*> (clip))
                    {
                        foundMidiClip = true;
                        CHECK_EQ (midiClip->getSequence().getNotes().size(), originalNoteCount);
                    }
                }
            }
            CHECK (foundMidiClip);

            // Verify audio clip exists
            bool foundAudioClip = false;
            for (auto* track : audioTracks)
            {
                for (auto* clip : track->getClips())
                {
                    if (dynamic_cast<WaveAudioClip*> (clip) != nullptr)
                    {
                        foundAudioClip = true;
                        break;
                    }
                }
            }
            CHECK (foundAudioClip);
        }

        cleanup();
    }

    TEST_CASE ("ProjectManager: create and retrieve both project types")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // Use a temp directory for the whole test
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // === Create a file-based project ===
        auto projectFile = tempDir.getChildFile ("pm_test_project.tracktion");
        ProjectManager::TempProject fileTp (pm, projectFile, true);
        auto fileProject = fileTp.project;

        REQUIRE (fileProject != nullptr);
        CHECK (fileProject->isValid());
        CHECK (fileProject->getProjectID().isValid());
        CHECK (fileProject->getProjectFile() == projectFile);

        // === Create a folder-based project ===
        auto projectFolder = tempDir.getChildFile ("pm_test_folder_project");
        projectFolder.createDirectory();

        ProjectManager::TempProject folderTp (pm, projectFolder, false);
        auto folderProject = folderTp.project;

        REQUIRE (folderProject != nullptr);
        CHECK (folderProject->isValid());
        CHECK (folderProject->getProjectID().isValid());
        CHECK (folderProject->getProjectFile() == projectFolder);

        // === Add both to the project list ===
        auto activeFolder = pm.getActiveProjectsFolder();
        auto addedFile = pm.addProjectToList (projectFile, true, activeFolder);
        auto addedFolder = pm.addProjectToList (projectFolder, true, activeFolder);

        CHECK (addedFile != nullptr);
        CHECK (addedFolder != nullptr);

        // === Test getProject(File) ===
        auto foundFile = pm.getProject (projectFile);
        auto foundFolder = pm.getProject (projectFolder);

        CHECK (foundFile != nullptr);
        CHECK (foundFolder != nullptr);

        // === Test getAllProjects() ===
        auto allProjects = pm.getAllProjects();
        bool foundFileInAll = false;
        bool foundFolderInAll = false;

        for (auto* p : allProjects)
        {
            if (p->getProjectFile() == projectFile)
                foundFileInAll = true;
            if (p->getProjectFile() == projectFolder)
                foundFolderInAll = true;
        }

        CHECK (foundFileInAll);
        CHECK (foundFolderInAll);

        // === Test findProjectWithFile() ===
        auto foundFileBySearch = pm.findProjectWithFile (activeFolder, projectFile);
        auto foundFolderBySearch = pm.findProjectWithFile (activeFolder, projectFolder);

        CHECK (foundFileBySearch != nullptr);
        CHECK (foundFolderBySearch != nullptr);

        // === Test findProjectWithId() — works for file-based (has a real ID) ===
        auto fileId = fileProject->getProjectID();
        auto foundById = pm.findProjectWithId (activeFolder, fileId);
        CHECK (foundById != nullptr);

        // Folder-based project has a hash-based ID — findProjectWithId matches on ID equality
        auto foundByFolderId = pm.findProjectWithId (activeFolder, folderProject->getProjectID());
        CHECK (foundByFolderId != nullptr);

        cleanup();
    }

    TEST_CASE ("Project: convert file-based to folder-based")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // Use a temp directory for the whole test
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a file-based project
        auto projectFile = tempDir.getChildFile ("convert_project.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        REQUIRE (project->isValid());

        project->setDescription ("Test project for conversion");

        // Create a sin wave file and copy it into the project directory so it's found after conversion
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);
        REQUIRE (sinFile->getFile().existsAsFile());

        auto sinWavFile = tempDir.getChildFile (sinFile->getFile().getFileName());
        sinFile->getFile().copyFileTo (sinWavFile);
        REQUIRE (sinWavFile.existsAsFile());

        // Create an edit with an audio clip
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        {
            auto edit = createEmptyEdit (engine, editItem->getSourceFile());
            edit->setProjectItemRef (editItem->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (1);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 1);

            auto waveClip = insertWaveClip (*audioTracks[0], "ConvertTestWave",
                                            sinWavFile,
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            // File-based project: source is a ProjectItemID
            REQUIRE (waveClip->getSourceFileReference().isUsingProjectReference());
            REQUIRE (waveClip->getSourceFileReference().getFile() == sinWavFile);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        // Capture old state
        auto oldName = project->getName();
        auto oldDesc = project->getDescription();
        auto oldProjectId = project->getProjectID();
        auto projectDir = project->getDefaultDirectory();

        struct ItemInfo
        {
            juce::String type;
            juce::File sourceFile;
        };
        std::vector<ItemInfo> oldItems;
        for (int i = 0; i < project->getNumProjectItems(); ++i)
        {
            if (auto item = project->getProjectItemAt (i))
                oldItems.push_back ({ item->getType(), item->getSourceFile() });
        }

        // Verify pre-conversion: edit source should be a ProjectItemID
        {
            auto editState = loadEditFromFile (engine, editItem->getSourceFile(), ProjectItemID{});
            bool foundProjectItemIdSource = false;

            std::function<void (const juce::ValueTree&)> checkTree = [&] (const juce::ValueTree& v)
            {
                if (v.hasProperty (IDs::source))
                {
                    auto src = v[IDs::source].toString();
                    if (ProjectItemID (src).isValid())
                        foundProjectItemIdSource = true;
                }

                for (int i = 0; i < v.getNumChildren(); ++i)
                    checkTree (v.getChild (i));
            };

            checkTree (editState);
            CHECK (foundProjectItemIdSource);
        }

        // Convert
        auto newProject = convertToFolderBasedProject (*project);
        REQUIRE (newProject != nullptr);

        // Verify .tracktion file was deleted
        CHECK_FALSE (projectFile.existsAsFile());

        // Verify project_info.json
        auto infoFile = projectDir.getChildFile ("project_info.json");
        CHECK (infoFile.existsAsFile());

        {
            auto infoJson = juce::JSON::parse (infoFile);
            CHECK (infoJson.isObject());

            if (auto* obj = infoJson.getDynamicObject())
            {
                CHECK_EQ (obj->getProperty ("name").toString().toStdString(), oldName.toStdString());
                CHECK_EQ (obj->getProperty ("description").toString().toStdString(), oldDesc.toStdString());
                CHECK_EQ ((int) obj->getProperty ("projectId"), oldProjectId.toInt());
            }
        }

        // Verify items exist in new project
        for (auto& oldItem : oldItems)
        {
            auto found = newProject->getProjectItemForFile (oldItem.sourceFile);
            CHECK_MESSAGE (found != nullptr, "Missing item for: " << oldItem.sourceFile.getFullPathName());

            if (found != nullptr)
                CHECK_EQ (found->getType().toStdString(), oldItem.type.toStdString());
        }

        // Verify edit sources are no longer ProjectItemIDs
        {
            auto editFile = editItem->getSourceFile();
            auto editState = loadEditFromFile (engine, editFile, ProjectItemID{});
            bool foundRelativePath = false;

            std::function<void (const juce::ValueTree&)> checkTree = [&] (const juce::ValueTree& v)
            {
                if (v.hasProperty (IDs::source))
                {
                    auto src = v[IDs::source].toString();

                    if (src.isNotEmpty() && ! ProjectItemID (src).isValid())
                        foundRelativePath = true;
                }

                for (int i = 0; i < v.getNumChildren(); ++i)
                    checkTree (v.getChild (i));
            };

            checkTree (editState);
            CHECK (foundRelativePath);
        }

        // Verify the edit loads correctly and audio clip resolves
        {
            auto loadedEdit = loadEditFromFile (engine, editItem->getSourceFile());
            REQUIRE (loadedEdit != nullptr);

            bool foundAudioClip = false;

            for (auto track : getAudioTracks (*loadedEdit))
            {
                for (auto clip : track->getClips())
                {
                    if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    {
                        foundAudioClip = true;
                        auto audioFile = waveClip->getAudioFile();
                        CHECK (audioFile.getFile().existsAsFile());

                        // Files inside of the project folder should be relative
                        REQUIRE (! waveClip->getSourceFileReference().isUsingProjectReference());
                        REQUIRE (! juce::File::isAbsolutePath (waveClip->getSourceFileReference().source.get()));
                        REQUIRE (waveClip->getSourceFileReference().getFile() == sinWavFile);
                    }
                }
            }

            CHECK (foundAudioClip);
        }

        newProject.reset(); // Reset the project ptr before the dir is deleted
        cleanup();
    }

    TEST_CASE ("Project: sourceFileMoved updates edit references")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto runTest = [&] (bool fileBased)
        {
            auto tempDir = juce::File::createTempFile ({});
            tempDir.createDirectory();

            auto cleanup = [&tempDir]
            {
                tempDir.deleteRecursively (false);
            };

            // Create the project
            juce::File projectPath;
            bool createNewId = false;

            if (fileBased)
            {
                projectPath = tempDir.getChildFile ("test_project.tracktion");
                createNewId = true;
            }
            else
            {
                projectPath = tempDir.getChildFile ("test_folder_project");
                projectPath.createDirectory();
                createNewId = false;
            }

            ProjectManager::TempProject tp (pm, projectPath, createNewId);
            auto project = tp.project;
            REQUIRE (project != nullptr);
            REQUIRE (project->isValid());

            // Create a wav file inside the project directory
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
            REQUIRE (sinFile != nullptr);

            auto projectDir = project->getDefaultDirectory();
            auto wavFile = projectDir.getChildFile ("source_audio.wav");
            sinFile->getFile().copyFileTo (wavFile);
            REQUIRE (wavFile.existsAsFile());

            // Create 2 edits
            auto editItem1 = project->createNewEdit();
            auto editItem2 = project->createNewEdit();
            REQUIRE (editItem1 != nullptr);
            REQUIRE (editItem2 != nullptr);

            // -- Edit 1 (will remain "open" during the move) --
            auto edit1 = createEmptyEdit (engine, editItem1->getSourceFile());
            edit1->setProjectItemRef (editItem1->getProjectItemRef());
            edit1->ensureNumberOfAudioTracks (1);

            {
                auto audioTracks = getAudioTracks (*edit1);
                REQUIRE (audioTracks.size() >= 1);

                auto waveClip = insertWaveClip (*audioTracks[0], "OpenClip",
                                                wavFile,
                                                { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                                DeleteExistingClips::no);
                REQUIRE (waveClip != nullptr);
                CHECK (waveClip->getSourceFileReference().getFile() == wavFile);
            }

            CHECK (test_utilities::saveEditSync (*edit1));

            // -- Edit 2 (will be closed during the move) --
            {
                auto edit2 = createEmptyEdit (engine, editItem2->getSourceFile());
                edit2->setProjectItemRef (editItem2->getProjectItemRef());
                edit2->ensureNumberOfAudioTracks (1);

                auto audioTracks = getAudioTracks (*edit2);
                REQUIRE (audioTracks.size() >= 1);

                auto waveClip = insertWaveClip (*audioTracks[0], "ClosedClip",
                                                wavFile,
                                                { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                                DeleteExistingClips::no);
                REQUIRE (waveClip != nullptr);
                CHECK (waveClip->getSourceFileReference().getFile() == wavFile);

                CHECK (test_utilities::saveEditSync (*edit2));
            }
            // edit2 is now destroyed (simulates a closed edit)

            project->save();

            // Move the source file
            auto newWavFile = projectDir.getChildFile ("moved_audio.wav");
            CHECK (wavFile.moveFileTo (newWavFile));
            REQUIRE (newWavFile.existsAsFile());
            REQUIRE (! wavFile.existsAsFile());

            // For file-based projects, update the ProjectItem source
            if (fileBased)
            {
                if (auto audioItem = project->getProjectItemForFile (wavFile))
                    audioItem->setSourceFile (newWavFile);
            }

            // Call sourceFileMoved
            project->sourceFileMoved (wavFile, newWavFile);

            // Verify the open edit (edit1) now points to the new file
            {
                bool foundUpdated = false;

                for (auto track : getAudioTracks (*edit1))
                {
                    for (auto clip : track->getClips())
                    {
                        if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                        {
                            foundUpdated = true;
                            CHECK (waveClip->getSourceFileReference().getFile() == newWavFile);
                        }
                    }
                }

                CHECK (foundUpdated);
            }

            // Destroy the open edit before checking the closed one
            edit1.reset();

            // Verify the closed edit (edit2) was updated on disk
            {
                auto loadedEdit = loadEditFromFile (engine, editItem2->getSourceFile());
                REQUIRE (loadedEdit != nullptr);

                bool foundUpdated = false;

                for (auto track : getAudioTracks (*loadedEdit))
                {
                    for (auto clip : track->getClips())
                    {
                        if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                        {
                            foundUpdated = true;
                            CHECK (waveClip->getSourceFileReference().getFile() == newWavFile);
                        }
                    }
                }

                CHECK (foundUpdated);
            }

            cleanup();
        };

        SUBCASE ("file-based project")
        {
            runTest (true);
        }

        SUBCASE ("folder-based project")
        {
            runTest (false);
        }
    }

    TEST_CASE ("Project: get and set description")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project")
        {
            auto projectFile = tempDir.getChildFile ("desc_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK (project->getDescription().isEmpty());

            project->setDescription ("Hello world");
            CHECK (project->getDescription() == "Hello world");

            // Verify persistence
            project->save();
            project->refreshProjectPropertiesFromFile();
            CHECK (project->getDescription() == "Hello world");
        }

        SUBCASE ("folder-based project")
        {
            auto projectFolder = tempDir.getChildFile ("desc_folder_test");
            projectFolder.createDirectory();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK (project->getDescription().isEmpty());

            project->setDescription ("Folder description");
            CHECK (project->getDescription() == "Folder description");

            // Verify persistence via project_info.json
            project->refreshProjectPropertiesFromFile();
            CHECK (project->getDescription() == "Folder description");
        }

        SUBCASE ("truncation at 8192 characters")
        {
            auto projectFile = tempDir.getChildFile ("desc_trunc.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            auto longDesc = juce::String::repeatedString ("x", 10000);
            project->setDescription (longDesc);
            CHECK (project->getDescription().length() == 8192);
        }

        cleanup();
    }

    TEST_CASE ("ProjectUtilities: consolidate single Edit")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a file-based project
        auto projectFile = tempDir.getChildFile ("consolidate_project.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        REQUIRE (project->isValid());

        // Create a sin wave file outside the project folder
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);
        REQUIRE (sinFile->getFile().existsAsFile());
        REQUIRE (! sinFile->getFile().isAChildOf (project->getDefaultDirectory()));

        // Create an edit with an audio clip + a MIDI clip
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        {
            auto edit = createEmptyEdit (engine, editItem->getSourceFile());
            edit->setProjectItemRef (editItem->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            // Audio clip referencing external file
            auto waveClip = insertWaveClip (*audioTracks[0], "ExternalWave",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            // MIDI clip with 3 notes
            auto midiClip = insertMIDIClip (*audioTracks[1], "TestMIDI",
                                            { 0_tp, TimePosition::fromSeconds (4.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (1.0), 100, 0, nullptr);
            midiClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                             BeatDuration::fromBeats (0.5), 80, 0, nullptr);
            midiClip->getSequence().addNote (67, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (2.0), 127, 0, nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        // Load the saved edit for consolidation
        auto edit = loadEditFromFile (engine, editItem->getSourceFile());
        REQUIRE (edit != nullptr);
        edit->setProjectItemRef (editItem->getProjectItemRef());

        CHECK (ProjectUtilities::canConsolidateEdit (*edit));

        auto [numImported, error] = ProjectUtilities::consolidateEdit (*edit);
        CHECK (numImported > 0);
        CHECK (error.isEmpty());

        CHECK_FALSE (ProjectUtilities::canConsolidateEdit (*edit));

        // Save and reload to verify
        CHECK (test_utilities::saveEditSync (*edit));
        edit.reset();

        // Reload and verify content
        {
            auto loadedEdit = loadEditFromFile (engine, editItem->getSourceFile());
            REQUIRE (loadedEdit != nullptr);

            auto projectDir = project->getDefaultDirectory();
            auto audioTracks = getAudioTracks (*loadedEdit);
            CHECK (audioTracks.size() >= 2);

            // Verify audio clip source is now inside project dir
            bool foundAudioClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    {
                        foundAudioClip = true;
                        auto sourceFile = waveClip->getSourceFileReference().getFile();
                        CHECK (sourceFile.isAChildOf (projectDir));
                        CHECK (sourceFile.existsAsFile());
                    }
                }
            }
            CHECK (foundAudioClip);

            // Verify MIDI clip still has 3 notes
            bool foundMidiClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto midiClip = dynamic_cast<MidiClip*> (clip))
                    {
                        foundMidiClip = true;
                        CHECK (midiClip->getSequence().getNotes().size() == 3);
                    }
                }
            }
            CHECK (foundMidiClip);
        }

        cleanup();
    }

    TEST_CASE ("ProjectUtilities: consolidate Project with multiple Edits")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a file-based project
        auto projectFile = tempDir.getChildFile ("consolidate_multi_project.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        REQUIRE (project->isValid());

        // Create a sin wave file outside the project folder
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);
        REQUIRE (sinFile->getFile().existsAsFile());
        REQUIRE (! sinFile->getFile().isAChildOf (project->getDefaultDirectory()));

        // Create two edits
        auto editItem1 = project->createNewEdit();
        auto editItem2 = project->createNewEdit();
        REQUIRE (editItem1 != nullptr);
        REQUIRE (editItem2 != nullptr);

        // -- Edit 1: audio clip (external) + MIDI clip with 3 notes --
        {
            auto edit = createEmptyEdit (engine, editItem1->getSourceFile());
            edit->setProjectItemRef (editItem1->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            auto waveClip = insertWaveClip (*audioTracks[0], "ExtWave1",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (2.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            auto midiClip = insertMIDIClip (*audioTracks[1], "Midi1",
                                            { 0_tp, TimePosition::fromSeconds (4.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (1.0), 100, 0, nullptr);
            midiClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                             BeatDuration::fromBeats (0.5), 80, 0, nullptr);
            midiClip->getSequence().addNote (67, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (2.0), 127, 0, nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        // -- Edit 2: audio clip (same external file) + MIDI clip with 2 notes --
        {
            auto edit = createEmptyEdit (engine, editItem2->getSourceFile());
            edit->setProjectItemRef (editItem2->getProjectItemRef());

            edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*edit);
            REQUIRE (audioTracks.size() >= 2);

            auto waveClip = insertWaveClip (*audioTracks[0], "ExtWave2",
                                            sinFile->getFile(),
                                            { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                            DeleteExistingClips::no);
            CHECK (waveClip != nullptr);

            auto midiClip = insertMIDIClip (*audioTracks[1], "Midi2",
                                            { 0_tp, TimePosition::fromSeconds (3.0) });
            REQUIRE (midiClip != nullptr);
            midiClip->getSequence().addNote (72, BeatPosition::fromBeats (0.0),
                                             BeatDuration::fromBeats (2.0), 90, 0, nullptr);
            midiClip->getSequence().addNote (76, BeatPosition::fromBeats (2.0),
                                             BeatDuration::fromBeats (1.0), 110, 0, nullptr);

            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        CHECK (ProjectUtilities::canConsolidateProject (*project));

        auto [numImported, error] = ProjectUtilities::consolidateProject (*project);
        CHECK (numImported > 0);
        CHECK (error.isEmpty());

        CHECK_FALSE (ProjectUtilities::canConsolidateProject (*project));

        auto projectDir = project->getDefaultDirectory();

        // Reload Edit 1 and verify
        {
            auto loadedEdit = loadEditFromFile (engine, editItem1->getSourceFile());
            REQUIRE (loadedEdit != nullptr);

            auto audioTracks = getAudioTracks (*loadedEdit);
            CHECK (audioTracks.size() >= 2);

            bool foundAudioClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    {
                        foundAudioClip = true;
                        auto sourceFile = waveClip->getSourceFileReference().getFile();
                        CHECK (sourceFile.isAChildOf (projectDir));
                        CHECK (sourceFile.existsAsFile());
                    }
                }
            }
            CHECK (foundAudioClip);

            bool foundMidiClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto midiClip = dynamic_cast<MidiClip*> (clip))
                    {
                        foundMidiClip = true;
                        CHECK (midiClip->getSequence().getNotes().size() == 3);
                    }
                }
            }
            CHECK (foundMidiClip);
        }

        // Reload Edit 2 and verify
        {
            auto loadedEdit = loadEditFromFile (engine, editItem2->getSourceFile());
            REQUIRE (loadedEdit != nullptr);

            auto audioTracks = getAudioTracks (*loadedEdit);
            CHECK (audioTracks.size() >= 2);

            bool foundAudioClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    {
                        foundAudioClip = true;
                        auto sourceFile = waveClip->getSourceFileReference().getFile();
                        CHECK (sourceFile.isAChildOf (projectDir));
                        CHECK (sourceFile.existsAsFile());
                    }
                }
            }
            CHECK (foundAudioClip);

            bool foundMidiClip = false;
            for (auto track : audioTracks)
            {
                for (auto clip : track->getClips())
                {
                    if (auto midiClip = dynamic_cast<MidiClip*> (clip))
                    {
                        foundMidiClip = true;
                        CHECK (midiClip->getSequence().getNotes().size() == 2);
                    }
                }
            }
            CHECK (foundMidiClip);
        }

        cleanup();
    }

    TEST_CASE ("Project: setSourceFile triggers sourceFileMoved")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto runTest = [&] (bool fileBased)
        {
            auto tempDir = juce::File::createTempFile ({});
            tempDir.createDirectory();

            auto cleanup = [&tempDir]
            {
                tempDir.deleteRecursively (false);
            };

            // Create the project
            juce::File projectPath;
            bool createNewId = false;

            if (fileBased)
            {
                projectPath = tempDir.getChildFile ("test_project.tracktion");
                createNewId = true;
            }
            else
            {
                projectPath = tempDir.getChildFile ("test_folder_project");
                projectPath.createDirectory();
                createNewId = false;
            }

            ProjectManager::TempProject tp (pm, projectPath, createNewId);
            auto project = tp.project;
            REQUIRE (project != nullptr);
            REQUIRE (project->isValid());

            // Create a wav file inside the project directory
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
            REQUIRE (sinFile != nullptr);

            auto projectDir = project->getDefaultDirectory();
            auto wavFile = projectDir.getChildFile ("source_audio.wav");
            sinFile->getFile().copyFileTo (wavFile);
            REQUIRE (wavFile.existsAsFile());

            // Create a ProjectItem for the wav file
            auto audioItem = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                                     "source_audio", {},
                                                     ProjectItem::Category::imported, false);
            REQUIRE (audioItem != nullptr);

            // Create 2 edits
            auto editItem1 = project->createNewEdit();
            auto editItem2 = project->createNewEdit();
            REQUIRE (editItem1 != nullptr);
            REQUIRE (editItem2 != nullptr);

            // -- Edit 1 (will remain "open" during the move) --
            auto edit1 = createEmptyEdit (engine, editItem1->getSourceFile());
            edit1->setProjectItemRef (editItem1->getProjectItemRef());
            edit1->ensureNumberOfAudioTracks (1);

            {
                auto audioTracks = getAudioTracks (*edit1);
                REQUIRE (audioTracks.size() >= 1);

                auto waveClip = insertWaveClip (*audioTracks[0], "OpenClip",
                                                wavFile,
                                                { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                                DeleteExistingClips::no);
                REQUIRE (waveClip != nullptr);
                CHECK (waveClip->getSourceFileReference().getFile() == wavFile);
            }

            CHECK (test_utilities::saveEditSync (*edit1));

            // -- Edit 2 (will be closed during the move) --
            {
                auto edit2 = createEmptyEdit (engine, editItem2->getSourceFile());
                edit2->setProjectItemRef (editItem2->getProjectItemRef());
                edit2->ensureNumberOfAudioTracks (1);

                auto audioTracks = getAudioTracks (*edit2);
                REQUIRE (audioTracks.size() >= 1);

                auto waveClip = insertWaveClip (*audioTracks[0], "ClosedClip",
                                                wavFile,
                                                { { 0_tp, TimePosition::fromSeconds (1.0) } },
                                                DeleteExistingClips::no);
                REQUIRE (waveClip != nullptr);
                CHECK (waveClip->getSourceFileReference().getFile() == wavFile);

                CHECK (test_utilities::saveEditSync (*edit2));
            }
            // edit2 is now destroyed (simulates a closed edit)

            project->save();

            // Move the source file
            auto newWavFile = projectDir.getChildFile ("moved_audio.wav");
            CHECK (wavFile.moveFileTo (newWavFile));
            REQUIRE (newWavFile.existsAsFile());
            REQUIRE (! wavFile.existsAsFile());

            // Call ONLY setSourceFile — this should trigger sourceFileMoved internally
            audioItem->setSourceFile (newWavFile);

            // Verify the open edit (edit1) now points to the new file
            {
                bool foundUpdated = false;

                for (auto track : getAudioTracks (*edit1))
                {
                    for (auto clip : track->getClips())
                    {
                        if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                        {
                            foundUpdated = true;
                            CHECK (waveClip->getSourceFileReference().getFile() == newWavFile);
                        }
                    }
                }

                CHECK (foundUpdated);
            }

            // Destroy the open edit before checking the closed one
            edit1.reset();

            // Verify the closed edit (edit2) was updated on disk
            {
                auto loadedEdit = loadEditFromFile (engine, editItem2->getSourceFile());
                REQUIRE (loadedEdit != nullptr);

                bool foundUpdated = false;

                for (auto track : getAudioTracks (*loadedEdit))
                {
                    for (auto clip : track->getClips())
                    {
                        if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                        {
                            foundUpdated = true;
                            CHECK (waveClip->getSourceFileReference().getFile() == newWavFile);
                        }
                    }
                }

                CHECK (foundUpdated);
            }

            cleanup();
        };

        SUBCASE ("file-based project")
        {
            runTest (true);
        }

        SUBCASE ("folder-based project")
        {
            runTest (false);
        }
    }

    TEST_CASE ("ProjectID: basic operations")
    {
        // Default-constructed is invalid
        ProjectID defaultId;
        CHECK_FALSE (defaultId.isValid());
        CHECK (defaultId.toInt() == 0);

        // Explicit construction
        ProjectID id1 (42);
        CHECK (id1.isValid());
        CHECK (id1.toInt() == 42);

        // Zero is invalid
        ProjectID zero (0);
        CHECK_FALSE (zero.isValid());

        // Equality
        ProjectID id2 (42);
        CHECK (id1 == id2);
        CHECK_FALSE (id1 != id2);

        // Inequality
        ProjectID id3 (99);
        CHECK (id1 != id3);
        CHECK_FALSE (id1 == id3);

        // Negative values are valid (non-zero)
        ProjectID neg (-1);
        CHECK (neg.isValid());
        CHECK (neg.toInt() == -1);
    }

    TEST_CASE ("ProjectItemID: construction and parsing")
    {
        SUBCASE ("default construction is invalid")
        {
            ProjectItemID pid;
            CHECK_FALSE (pid.isValid());
            CHECK (pid.isInvalid());
            CHECK (pid.getRawID() == 0);
        }

        SUBCASE ("construct from ints")
        {
            ProjectItemID pid (5, ProjectID (10));
            CHECK (pid.isValid());
            CHECK (pid.getItemID() == 5);
            CHECK (pid.getProjectID() == ProjectID (10));
        }

        SUBCASE ("round-trip via toString")
        {
            ProjectItemID original (0x1234, ProjectID (0xABCD));
            auto str = original.toString();
            ProjectItemID parsed (str);

            CHECK (parsed.isValid());
            CHECK (parsed == original);
            CHECK (parsed.getItemID() == original.getItemID());
            CHECK (parsed.getProjectID() == original.getProjectID());
        }

        SUBCASE ("round-trip via toStringSuitableForFilename")
        {
            ProjectItemID original (0x5678, ProjectID (0xDEF0));
            auto fnStr = original.toStringSuitableForFilename();

            // The filename format uses '_' instead of '/'
            CHECK (fnStr.contains ("_"));
            CHECK_FALSE (fnStr.contains ("/"));

            // Should be parseable back
            ProjectItemID parsed (fnStr);
            CHECK (parsed == original);
        }

        SUBCASE ("parse invalid string returns invalid")
        {
            ProjectItemID empty { juce::String{} };
            CHECK_FALSE (empty.isValid());

            ProjectItemID garbage (juce::String ("not_a_hex_id!"));
            CHECK_FALSE (garbage.isValid());

            ProjectItemID justText (juce::String ("hello"));
            CHECK_FALSE (justText.isValid());
        }

        SUBCASE ("createNewID produces valid IDs")
        {
            auto pid = ProjectItemID::createNewID (ProjectID (42));
            CHECK (pid.isValid());
            CHECK (pid.getProjectID() == ProjectID (42));
        }

        SUBCASE ("withNewProjectID preserves item ID")
        {
            ProjectItemID pid (0x100, ProjectID (0x200));
            auto changed = pid.withNewProjectID (ProjectID (0x300));

            CHECK (changed.getItemID() == pid.getItemID());
            CHECK (changed.getProjectID() == ProjectID (0x300));
            CHECK (changed != pid);
        }

        SUBCASE ("fromProperty")
        {
            ProjectItemID original (0x42, ProjectID (0x99));
            juce::ValueTree vt ("test");
            vt.setProperty ("myProp", original.toString(), nullptr);

            auto parsed = ProjectItemID::fromProperty (vt, "myProp");
            CHECK (parsed == original);
        }
    }

    TEST_CASE ("ProjectItemRef: type detection and factory methods")
    {
        SUBCASE ("default is invalid")
        {
            ProjectItemRef ref;
            CHECK_FALSE (ref.isValid());
            CHECK_FALSE (ref.isProjectItemID());
            CHECK_FALSE (ref.isRelativePath());
            CHECK_FALSE (ref.isAbsolutePath());
        }

        SUBCASE ("from ProjectItemID")
        {
            auto pid = ProjectItemID (0x100, ProjectID (0x200));
            ProjectItemRef ref (pid);

            CHECK (ref.isValid());
            CHECK (ref.isProjectItemID());
            CHECK_FALSE (ref.isRelativePath());
            CHECK_FALSE (ref.isAbsolutePath());
            CHECK (ref.getProjectItemID().has_value());
            CHECK (ref.getProjectItemID().value() == pid);
            CHECK (ref.getProjectID() == pid.getProjectID());
        }

        SUBCASE ("from relative path")
        {
            auto ref = ProjectItemRef::fromPath ("audio/take1.wav");

            CHECK (ref.isValid());
            CHECK_FALSE (ref.isProjectItemID());
            CHECK (ref.isRelativePath());
            CHECK_FALSE (ref.isAbsolutePath());
            CHECK_FALSE (ref.getProjectItemID().has_value());
        }

        SUBCASE ("from absolute path")
        {
            auto absFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("test.wav");
            auto ref = ProjectItemRef::fromAbsolutePath (absFile);

            CHECK (ref.isValid());
            CHECK_FALSE (ref.isProjectItemID());
            CHECK_FALSE (ref.isRelativePath());
            CHECK (ref.isAbsolutePath());
        }

        SUBCASE ("toString round-trip")
        {
            auto pid = ProjectItemID (0x42, ProjectID (0x99));
            ProjectItemRef ref (pid);

            CHECK (ref.toString() == pid.toString());
        }

        SUBCASE ("toIDForFilename with ProjectItemID")
        {
            auto pid = ProjectItemID (0x42, ProjectID (0x99));
            ProjectItemRef ref (pid);

            CHECK (ref.toIDForFilename() == pid.toStringSuitableForFilename());
        }

        SUBCASE ("toIDForFilename with path uses hash")
        {
            auto ref = ProjectItemRef::fromPath ("audio/take1.wav");
            auto id = ref.toIDForFilename();
            CHECK (id.isNotEmpty());

            // Same path should give same hash
            auto ref2 = ProjectItemRef::fromPath ("audio/take1.wav");
            CHECK (ref2.toIDForFilename() == id);
        }

        SUBCASE ("equality operators")
        {
            auto pid = ProjectItemID (0x42, ProjectID (0x99));
            ProjectItemRef ref1 (pid);
            ProjectItemRef ref2 (pid);

            CHECK (ref1 == ref2);
            CHECK_FALSE (ref1 != ref2);

            auto ref3 = ProjectItemRef::fromPath ("audio.wav");
            CHECK (ref1 != ref3);
        }

        SUBCASE ("ProjectItemID comparison operators")
        {
            auto pid = ProjectItemID (0x42, ProjectID (0x99));
            ProjectItemRef ref (pid);

            CHECK (ref == pid);
            CHECK_FALSE (ref != pid);
            CHECK (pid == ref);
            CHECK_FALSE (pid != ref);

            auto otherPid = ProjectItemID (0x43, ProjectID (0x99));
            CHECK (ref != otherPid);
        }

        SUBCASE ("resolve absolute path")
        {
            auto& engine = *Engine::getEngines()[0];
            auto absFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("test_resolve.wav");
            auto ref = ProjectItemRef::fromAbsolutePath (absFile);

            auto resolved = ref.resolve (engine);
            CHECK (resolved == absFile);
        }

        SUBCASE ("resolve relative path with folder")
        {
            auto& engine = *Engine::getEngines()[0];
            auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory);
            auto ref = ProjectItemRef::fromPath ("subdir/audio.wav");

            auto resolved = ref.resolve (engine, folder);
            CHECK (resolved == folder.getChildFile ("subdir/audio.wav"));
        }

        SUBCASE ("resolve empty ref returns empty")
        {
            auto& engine = *Engine::getEngines()[0];
            ProjectItemRef empty;
            CHECK (empty.resolve (engine) == juce::File());
        }
    }

    TEST_CASE ("FolderBasedProject: inferCategory covers all subdirectories")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("cat_test_project");
        projectFolder.createDirectory();

        // Create files in various category subdirectories
        auto createFile = [&] (const juce::String& subDir, const juce::String& filename)
        {
            auto dir = projectFolder.getChildFile (subDir);
            dir.createDirectory();
            auto f = dir.getChildFile (filename);
            f.create();
            return f;
        };

        createFile ("Recorded", "take1.wav");
        createFile ("Exported", "mix.wav");
        createFile ("Imported", "sample.wav");
        createFile ("Rendered", "render.wav");
        createFile ("Frozen", "frozen_track.wav");
        createFile ("Archived", "old.wav");
        createFile ("Movies", "video.mp4");
        createFile ("Other", "misc.wav");

        // Also a root-level wav, video and edit
        projectFolder.getChildFile ("root.wav").create();
        projectFolder.getChildFile ("root.mp4").create();
        projectFolder.getChildFile ("my_edit.tracktionedit").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Check category inference
        auto recordedItem = project->getProjectItemForFile (projectFolder.getChildFile ("Recorded").getChildFile ("take1.wav"));
        REQUIRE (recordedItem != nullptr);
        CHECK (recordedItem->getCategory() == ProjectItem::Category::recorded);

        auto exportedItem = project->getProjectItemForFile (projectFolder.getChildFile ("Exported").getChildFile ("mix.wav"));
        REQUIRE (exportedItem != nullptr);
        CHECK (exportedItem->getCategory() == ProjectItem::Category::exports);

        auto importedItem = project->getProjectItemForFile (projectFolder.getChildFile ("Imported").getChildFile ("sample.wav"));
        REQUIRE (importedItem != nullptr);
        CHECK (importedItem->getCategory() == ProjectItem::Category::imported);

        auto renderedItem = project->getProjectItemForFile (projectFolder.getChildFile ("Rendered").getChildFile ("render.wav"));
        REQUIRE (renderedItem != nullptr);
        CHECK (renderedItem->getCategory() == ProjectItem::Category::rendered);

        auto frozenItem = project->getProjectItemForFile (projectFolder.getChildFile ("Frozen").getChildFile ("frozen_track.wav"));
        REQUIRE (frozenItem != nullptr);
        CHECK (frozenItem->getCategory() == ProjectItem::Category::frozen);

        auto archivedItem = project->getProjectItemForFile (projectFolder.getChildFile ("Archived").getChildFile ("old.wav"));
        REQUIRE (archivedItem != nullptr);
        CHECK (archivedItem->getCategory() == ProjectItem::Category::archives);

        auto videoItem = project->getProjectItemForFile (projectFolder.getChildFile ("Movies").getChildFile ("video.mp4"));
        REQUIRE (videoItem != nullptr);
        CHECK (videoItem->getCategory() == ProjectItem::Category::video);

        // "Other" directory maps to Category::none
        auto otherItem = project->getProjectItemForFile (projectFolder.getChildFile ("Other").getChildFile ("misc.wav"));
        REQUIRE (otherItem != nullptr);
        CHECK (otherItem->getCategory() == ProjectItem::Category::none);

        // Root-level audio maps to Category::recorded so that it appears in the media list
        auto rootWav = project->getProjectItemForFile (projectFolder.getChildFile ("root.wav"));
        REQUIRE (rootWav != nullptr);
        CHECK (rootWav->getCategory() == ProjectItem::Category::recorded);

        // Root-level video maps to Category::video
        auto rootVideo = project->getProjectItemForFile (projectFolder.getChildFile ("root.mp4"));
        REQUIRE (rootVideo != nullptr);
        CHECK (rootVideo->getCategory() == ProjectItem::Category::video);

        // Root-level edit maps to Category::edit
        auto rootEdit = project->getProjectItemForFile (projectFolder.getChildFile ("my_edit.tracktionedit"));
        REQUIRE (rootEdit != nullptr);
        CHECK (rootEdit->getCategory() == ProjectItem::Category::edit);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: file type inference")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("type_test_project");
        projectFolder.createDirectory();

        // Create files of different types
        projectFolder.getChildFile ("song.tracktionedit").create();
        projectFolder.getChildFile ("song2.trkedit").create();
        projectFolder.getChildFile ("audio.wav").create();
        projectFolder.getChildFile ("audio2.aiff").create();
        projectFolder.getChildFile ("audio3.mp3").create();
        projectFolder.getChildFile ("audio4.ogg").create();
        projectFolder.getChildFile ("audio5.flac").create();
        projectFolder.getChildFile ("drums.mid").create();
        projectFolder.getChildFile ("bass.midi").create();
        projectFolder.getChildFile ("clip.mp4").create();
        projectFolder.getChildFile ("clip2.mov").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("song.tracktionedit"))->getType() == ProjectItem::editItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("song2.trkedit"))->getType() == ProjectItem::editItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("audio.wav"))->getType() == ProjectItem::waveItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("audio2.aiff"))->getType() == ProjectItem::waveItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("audio3.mp3"))->getType() == ProjectItem::waveItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("audio4.ogg"))->getType() == ProjectItem::waveItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("audio5.flac"))->getType() == ProjectItem::waveItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("drums.mid"))->getType() == ProjectItem::midiItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("bass.midi"))->getType() == ProjectItem::midiItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("clip.mp4"))->getType() == ProjectItem::videoItemType());
        CHECK (project->getProjectItemForFile (projectFolder.getChildFile ("clip2.mov"))->getType() == ProjectItem::videoItemType());

        CHECK (project->getNumProjectItems() == 11);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: createDefaultFolders and getDirectoryForMedia")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("folders_test");
        projectFolder.createDirectory();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // createDefaultFolders should create the subdirectories
        project->createDefaultFolders();

        CHECK (projectFolder.getChildFile ("Archived").isDirectory());
        CHECK (projectFolder.getChildFile ("Exported").isDirectory());
        CHECK (projectFolder.getChildFile ("Imported").isDirectory());
        CHECK (projectFolder.getChildFile ("Recorded").isDirectory());
        CHECK (projectFolder.getChildFile ("Rendered").isDirectory());

        // getDirectoryForMedia returns appropriate directories
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::recorded) == projectFolder.getChildFile ("Recorded"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::exports) == projectFolder.getChildFile ("Exported"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::imported) == projectFolder.getChildFile ("Imported"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::rendered) == projectFolder.getChildFile ("Rendered"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::archives) == projectFolder.getChildFile ("Archived"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::frozen) == projectFolder.getChildFile ("Frozen"));
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::video) == projectFolder.getChildFile ("Movies"));

        // edit and none categories return the project folder itself
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::edit) == projectFolder);
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::none) == projectFolder);

        cleanup();
    }

    TEST_CASE ("Project: isFolderBased and getSourcePathForFile")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project is not folder-based")
        {
            auto projectFile = tempDir.getChildFile ("fb_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK_FALSE (project->isFolderBased());
        }

        SUBCASE ("folder-based project is folder-based")
        {
            auto projectFolder = tempDir.getChildFile ("fb_test_folder");
            projectFolder.createDirectory();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK (project->isFolderBased());
        }

        SUBCASE ("getSourcePathForFile returns relative for child files")
        {
            auto projectFolder = tempDir.getChildFile ("src_path_test");
            projectFolder.createDirectory();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            auto childFile = projectFolder.getChildFile ("subdir").getChildFile ("audio.wav");
            auto path = project->getSourcePathForFile (childFile);

            CHECK_FALSE (juce::File::isAbsolutePath (path));
            CHECK (path == "subdir/audio.wav");
        }

        SUBCASE ("getSourcePathForFile returns absolute for external files")
        {
            auto projectFolder = tempDir.getChildFile ("src_path_test2");
            projectFolder.createDirectory();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            auto externalFile = tempDir.getChildFile ("external.wav");
            auto path = project->getSourcePathForFile (externalFile);

            CHECK (juce::File::isAbsolutePath (path));
            CHECK (path == externalFile.getFullPathName());
        }

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: createNewItem deduplication")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("dedup_test");
        projectFolder.createDirectory();

        auto wavFile = projectFolder.getChildFile ("audio.wav");
        wavFile.create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // First creation
        auto item1 = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                              "audio", {}, ProjectItem::Category::none, false);
        REQUIRE (item1 != nullptr);

        // Second creation with same file and type should return the same item
        auto item2 = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                              "audio", {}, ProjectItem::Category::none, false);
        CHECK (item1 == item2);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: project properties persistence")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("prop_test");
        projectFolder.createDirectory();

        // Set and save properties
        {
            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            project->setProjectProperty ("myKey", "myValue");
            CHECK (project->getProjectProperty ("myKey") == "myValue");
            project->save();
        }

        // Reload and verify
        {
            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK (project->getProjectProperty ("myKey") == "myValue");
        }

        // Verify project_info.json exists
        CHECK (projectFolder.getChildFile ("project_info.json").existsAsFile());

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: removeProjectItem")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("removes item without deleting source file")
        {
            auto projectFolder = tempDir.getChildFile ("remove_test");
            projectFolder.createDirectory();
            auto sourceFile = projectFolder.getChildFile ("audio.wav");
            sourceFile.create();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);
            REQUIRE (project->getNumProjectItems() >= 1);

            auto ref = project->getProjectItemRef (0);
            CHECK (project->removeProjectItem (ref, false));
            CHECK (project->getNumProjectItems() == 0);
            CHECK (sourceFile.existsAsFile());
        }

        SUBCASE ("removes item and deletes source file")
        {
            auto projectFolder = tempDir.getChildFile ("remove_test2");
            projectFolder.createDirectory();
            auto sourceFile = projectFolder.getChildFile ("audio2.wav");
            sourceFile.create();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);
            REQUIRE (project->getNumProjectItems() >= 1);

            auto ref = project->getProjectItemRef (0);
            CHECK (project->removeProjectItem (ref, true));
            CHECK (project->getNumProjectItems() == 0);
            CHECK_FALSE (sourceFile.existsAsFile());
        }

        cleanup();
    }

    TEST_CASE ("Project: isTemporary flag")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project")
        {
            auto projectFile = tempDir.getChildFile ("temp_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            project->setTemporary (true);
            CHECK (project->isTemporary());

            project->setTemporary (false);
            CHECK_FALSE (project->isTemporary());
        }

        SUBCASE ("folder-based project")
        {
            auto projectFolder = tempDir.getChildFile ("temp_test_folder");
            projectFolder.createDirectory();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            project->setTemporary (true);
            CHECK (project->isTemporary());

            project->setTemporary (false);
            CHECK_FALSE (project->isTemporary());
        }

        cleanup();
    }

    TEST_CASE ("Project: setName")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project")
        {
            auto projectFile = tempDir.getChildFile ("name_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            project->setName ("New Name");
            CHECK (project->getName() == "New Name");

            project->save();
            project->refreshProjectPropertiesFromFile();
            CHECK (project->getName() == "New Name");
        }

        cleanup();
    }

    TEST_CASE ("Project: isReadOnly")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("readonly_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // A newly created project should not be read-only
        CHECK_FALSE (project->isReadOnly());

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: no-op methods")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("noop_test");
        projectFolder.createDirectory();
        projectFolder.getChildFile ("audio.wav").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        REQUIRE (project->getNumProjectItems() >= 1);

        // moveProjectItem is a no-op
        auto refBefore = project->getProjectItemRef (0);
        project->moveProjectItem (0, 0);
        CHECK (project->getProjectItemRef (0) == refBefore);

        // redirectIDsFromProject is a no-op
        project->redirectIDsFromProject (ProjectID (1), ProjectID (2));
        CHECK (project->getNumProjectItems() >= 1);

        // mergeArchiveContents is a no-op
        project->mergeArchiveContents (juce::File());

        // mergeOtherProjectIntoThis is a no-op
        project->mergeOtherProjectIntoThis (juce::File());

        // refreshFolderStructure is a no-op
        project->refreshFolderStructure();

        // createNewProjectId is a no-op
        auto idBefore = project->getProjectID();
        project->createNewProjectId();
        CHECK (project->getProjectID() == idBefore);

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: getProjectItemRef out of bounds")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("oob_test");
        projectFolder.createDirectory();
        projectFolder.getChildFile ("audio.wav").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Out of bounds returns invalid ref
        auto invalidRef = project->getProjectItemRef (-1);
        CHECK_FALSE (invalidRef.isValid());

        auto oobRef = project->getProjectItemRef (9999);
        CHECK_FALSE (oobRef.isValid());

        // Out of bounds getProjectItemAt returns nullptr
        CHECK (project->getProjectItemAt (-1) == nullptr);
        CHECK (project->getProjectItemAt (9999) == nullptr);

        cleanup();
    }

    TEST_CASE ("Project: convertToFolderBasedProject with invalid project returns nullptr")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // Create a folder-based project (not file-based)
        auto projectFolder = tempDir.getChildFile ("invalid_convert_test");
        projectFolder.createDirectory();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Converting a folder-based project should return nullptr
        // (it's already folder-based, getProjectFile() is a directory not a file)
        auto result = convertToFolderBasedProject (*project);
        CHECK (result == nullptr);

        cleanup();
    }

    TEST_CASE ("ProjectUtilities: isConsolidated wrappers")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("consolidated_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // A project with no external references should be consolidated
        CHECK (ProjectUtilities::isConsolidated (*project));

        // Create an edit with no external refs — should be consolidated
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        {
            auto edit = createEmptyEdit (engine, editItem->getSourceFile());
            edit->setProjectItemRef (editItem->getProjectItemRef());
            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        {
            auto edit = loadEditFromFile (engine, editItem->getSourceFile());
            REQUIRE (edit != nullptr);
            edit->setProjectItemRef (editItem->getProjectItemRef());
            CHECK (ProjectUtilities::isConsolidated (*edit));
        }

        cleanup();
    }

    TEST_CASE ("FileBasedProject: moveProjectItem reorders items")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("move_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Create 3 items
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile1 = project->getDefaultDirectory().getChildFile ("audio1.wav");
        auto wavFile2 = project->getDefaultDirectory().getChildFile ("audio2.wav");

        sinFile->getFile().copyFileTo (wavFile1);
        sinFile->getFile().copyFileTo (wavFile2);

        auto item1 = project->createNewItem (wavFile1, ProjectItem::waveItemType(),
                                              "audio1", {}, ProjectItem::Category::none, false);
        auto item2 = project->createNewItem (wavFile2, ProjectItem::waveItemType(),
                                              "audio2", {}, ProjectItem::Category::none, false);
        REQUIRE (item1 != nullptr);
        REQUIRE (item2 != nullptr);

        auto ref0 = project->getProjectItemRef (0);
        auto ref1 = project->getProjectItemRef (1);

        // Move item 0 to position 1
        project->moveProjectItem (0, 1);

        // After moving, item at position 0 should be what was at position 1
        CHECK (project->getProjectItemRef (0) == ref1);
        CHECK (project->getProjectItemRef (1) == ref0);

        cleanup();
    }

    TEST_CASE ("Project: lockFile and unlockFile")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("lock_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Should be callable without error
        project->lockFile();
        project->unlockFile();

        cleanup();
    }

    TEST_CASE ("Project: getSelectableDescription")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project")
        {
            auto projectFile = tempDir.getChildFile ("desc_sel_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            CHECK (project->getSelectableDescription().isNotEmpty());
        }

        cleanup();
    }

    TEST_CASE ("FileBasedProject: redirectIDsFromProject")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("redirect_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Create an edit with actual content so loadEditForExamining works
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        {
            auto edit = createEmptyEdit (engine, editItem->getSourceFile());
            edit->setProjectItemRef (editItem->getProjectItemRef());
            CHECK (test_utilities::saveEditSync (*edit));
        }

        project->save();

        auto currentId = project->getProjectID();

        // Redirect from a non-matching ID — should be a no-op (no items match)
        auto fakeOldId = ProjectID (999999);
        project->redirectIDsFromProject (fakeOldId, currentId);
        project->save();

        // Items should still have their original project ID
        for (int i = 0; i < project->getNumProjectItems(); ++i)
        {
            auto ref = project->getProjectItemRef (i);

            if (ref.isProjectItemID())
                CHECK (ref.getProjectID() == currentId);
        }

        cleanup();
    }

    TEST_CASE ("FolderBasedProject: setName renames folder")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("rename_test");
        projectFolder.createDirectory();
        projectFolder.getChildFile ("audio.wav").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);
        CHECK (project->getName() == "rename_test");

        project->setName ("renamed_project");
        CHECK (project->getName() == "renamed_project");

        // The folder should have moved
        CHECK (project->getProjectFile().getFileName() == "renamed_project");
        CHECK (project->getProjectFile().isDirectory());

        cleanup();
    }

    TEST_CASE ("FileBasedProject: removeProjectItem")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("remove_fb_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Create a wav file
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("audio_to_remove.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "audio_to_remove", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);

        auto numBefore = project->getNumProjectItems();
        auto ref = item->getProjectItemRef();

        CHECK (project->removeProjectItem (ref, false));
        CHECK (project->getNumProjectItems() == numBefore - 1);

        // Source file should still exist (deleteSourceMaterial was false)
        CHECK (wavFile.existsAsFile());

        cleanup();
    }

    TEST_CASE ("FileBasedProject: getDirectoryForMedia creates subdirectories")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("media_dir_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto projectDir = project->getDefaultDirectory();

        // getDirectoryForMedia should create the subdirectory
        auto recordedDir = project->getDirectoryForMedia (ProjectItem::Category::recorded);
        CHECK (recordedDir == projectDir.getChildFile ("Recorded"));
        CHECK (recordedDir.isDirectory());

        auto exportedDir = project->getDirectoryForMedia (ProjectItem::Category::exports);
        CHECK (exportedDir == projectDir.getChildFile ("Exported"));
        CHECK (exportedDir.isDirectory());

        auto importedDir = project->getDirectoryForMedia (ProjectItem::Category::imported);
        CHECK (importedDir == projectDir.getChildFile ("Imported"));
        CHECK (importedDir.isDirectory());

        auto renderedDir = project->getDirectoryForMedia (ProjectItem::Category::rendered);
        CHECK (renderedDir == projectDir.getChildFile ("Rendered"));
        CHECK (renderedDir.isDirectory());

        auto frozenDir = project->getDirectoryForMedia (ProjectItem::Category::frozen);
        CHECK (frozenDir == projectDir.getChildFile ("Frozen"));
        CHECK (frozenDir.isDirectory());

        auto archivedDir = project->getDirectoryForMedia (ProjectItem::Category::archives);
        CHECK (archivedDir == projectDir.getChildFile ("Archived"));
        CHECK (archivedDir.isDirectory());

        auto videoDir = project->getDirectoryForMedia (ProjectItem::Category::video);
        CHECK (videoDir == projectDir.getChildFile ("Movies"));
        CHECK (videoDir.isDirectory());

        // edit and none return the project dir itself
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::edit) == projectDir);
        CHECK (project->getDirectoryForMedia (ProjectItem::Category::none) == projectDir);

        cleanup();
    }

    TEST_CASE ("ProjectItem: getSelectableDescription")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("selectable_desc_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Create an edit item
        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);
        CHECK (editItem->getSelectableDescription().isNotEmpty());

        // Create a wave item
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("desc_audio.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto waveItem = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                                 "desc_audio", {}, ProjectItem::Category::none, false);
        REQUIRE (waveItem != nullptr);
        CHECK (waveItem->getSelectableDescription().isNotEmpty());

        cleanup();
    }

    TEST_CASE ("ProjectItem: isEdit, isWave, isMidi")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFolder = tempDir.getChildFile ("type_check_test");
        projectFolder.createDirectory();

        projectFolder.getChildFile ("song.tracktionedit").create();
        projectFolder.getChildFile ("audio.wav").create();
        projectFolder.getChildFile ("notes.mid").create();
        projectFolder.getChildFile ("video.mp4").create();

        ProjectManager::TempProject tp (pm, projectFolder, false);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto editItem = project->getProjectItemForFile (projectFolder.getChildFile ("song.tracktionedit"));
        REQUIRE (editItem != nullptr);
        CHECK (editItem->isEdit());
        CHECK_FALSE (editItem->isWave());
        CHECK_FALSE (editItem->isMidi());
        CHECK_FALSE (editItem->isVideo());

        auto waveItem = project->getProjectItemForFile (projectFolder.getChildFile ("audio.wav"));
        REQUIRE (waveItem != nullptr);
        CHECK_FALSE (waveItem->isEdit());
        CHECK (waveItem->isWave());
        CHECK_FALSE (waveItem->isMidi());
        CHECK_FALSE (waveItem->isVideo());

        auto midiItem = project->getProjectItemForFile (projectFolder.getChildFile ("notes.mid"));
        REQUIRE (midiItem != nullptr);
        CHECK_FALSE (midiItem->isEdit());
        CHECK_FALSE (midiItem->isWave());
        CHECK (midiItem->isMidi());
        CHECK_FALSE (midiItem->isVideo());

        auto videoItem = project->getProjectItemForFile (projectFolder.getChildFile ("video.mp4"));
        REQUIRE (videoItem != nullptr);
        CHECK_FALSE (videoItem->isEdit());
        CHECK_FALSE (videoItem->isWave());
        CHECK_FALSE (videoItem->isMidi());
        CHECK (videoItem->isVideo());

        cleanup();
    }

    TEST_CASE ("FileBasedProject: getSelectableDescription returns Project")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("sel_desc_fb.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // File-based project with write access should say "Project"
        CHECK (project->getSelectableDescription() == TRANS("Project"));

        cleanup();
    }

    TEST_CASE ("FileBasedProject: createDefaultFolders")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("default_folders_fb.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        project->createDefaultFolders();

        auto projectDir = project->getDefaultDirectory();
        CHECK (projectDir.getChildFile ("Archived").isDirectory());
        CHECK (projectDir.getChildFile ("Exported").isDirectory());
        CHECK (projectDir.getChildFile ("Imported").isDirectory());
        CHECK (projectDir.getChildFile ("Recorded").isDirectory());
        CHECK (projectDir.getChildFile ("Rendered").isDirectory());

        cleanup();
    }

    TEST_CASE ("ProjectItem: setSourceFile and getSourceFile")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("source_file_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("original.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "original", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);
        CHECK (item->getSourceFile() == wavFile);

        // Move the file and update
        auto newWavFile = project->getDefaultDirectory().getChildFile ("moved.wav");
        wavFile.moveFileTo (newWavFile);

        item->setSourceFile (newWavFile);
        CHECK (item->getSourceFile() == newWavFile);

        cleanup();
    }

    TEST_CASE ("ProjectItem: name and description")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("name_desc_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("name_test.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "original_name", "test desc",
                                             ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);
        CHECK (item->getName() == "original_name");
        CHECK (item->getDescription() == "test desc");

        item->setName ("new_name", {});
        CHECK (item->getName() == "new_name");

        item->setDescription ("new desc");
        CHECK (item->getDescription() == "new desc");

        cleanup();
    }

    TEST_CASE ("ProjectItem: category get/set")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("cat_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("cat_audio.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "cat_audio", {}, ProjectItem::Category::imported, false);
        REQUIRE (item != nullptr);

        CHECK (item->getCategory() == ProjectItem::Category::imported);

        item->setCategory (ProjectItem::Category::recorded);
        CHECK (item->getCategory() == ProjectItem::Category::recorded);

        item->setCategory (ProjectItem::Category::exports);
        CHECK (item->getCategory() == ProjectItem::Category::exports);

        cleanup();
    }

    TEST_CASE ("ProjectItem: copyAllPropertiesFrom")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("copy_props_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wav1 = project->getDefaultDirectory().getChildFile ("src.wav");
        auto wav2 = project->getDefaultDirectory().getChildFile ("dst.wav");
        sinFile->getFile().copyFileTo (wav1);
        sinFile->getFile().copyFileTo (wav2);

        auto src = project->createNewItem (wav1, ProjectItem::waveItemType(),
                                            "source_item", "source desc",
                                            ProjectItem::Category::none, false);
        auto dst = project->createNewItem (wav2, ProjectItem::waveItemType(),
                                            "dest_item", "dest desc",
                                            ProjectItem::Category::none, false);
        REQUIRE (src != nullptr);
        REQUIRE (dst != nullptr);

        dst->copyAllPropertiesFrom (*src);
        CHECK (dst->getName() == "source_item");
        CHECK (dst->getDescription() == "source desc");

        cleanup();
    }

    TEST_CASE ("ProjectItem: getFileName")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("filename_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("my_audio.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "my_audio", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);

        CHECK (item->getFileName() == "my_audio.wav");

        cleanup();
    }

    TEST_CASE ("ProjectItem: getLength and verifyLength")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("length_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("length_audio.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "length_audio", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);

        // verifyLength should detect the length from the file
        item->verifyLength();
        CHECK (item->getLength() > 0.0);
        CHECK (item->getLength() == doctest::Approx (2.0).epsilon (0.1));

        cleanup();
    }

    TEST_CASE ("ProjectItem: getProjectName")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("projname_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        project->setName ("TestProjectName");

        auto editItem = project->createNewEdit();
        REQUIRE (editItem != nullptr);

        CHECK (editItem->getProjectName() == "TestProjectName");

        cleanup();
    }

    TEST_CASE ("ProjectItem: hasBeenDeleted")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        SUBCASE ("file-based project item after removal")
        {
            auto projectFile = tempDir.getChildFile ("deleted_test.tracktion");
            ProjectManager::TempProject tp (pm, projectFile, true);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
            REQUIRE (sinFile != nullptr);

            auto wavFile = project->getDefaultDirectory().getChildFile ("to_delete.wav");
            sinFile->getFile().copyFileTo (wavFile);

            auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                                 "to_delete", {}, ProjectItem::Category::none, false);
            REQUIRE (item != nullptr);
            CHECK_FALSE (item->hasBeenDeleted());

            // Remove from project
            auto ref = item->getProjectItemRef();
            project->removeProjectItem (ref, false);

            // Now it should be deleted
            CHECK (item->hasBeenDeleted());
        }

        SUBCASE ("folder-based project item with existing file")
        {
            auto projectFolder = tempDir.getChildFile ("deleted_folder_test");
            projectFolder.createDirectory();

            auto wavFile = projectFolder.getChildFile ("audio.wav");
            wavFile.create();

            ProjectManager::TempProject tp (pm, projectFolder, false);
            auto project = tp.project;
            REQUIRE (project != nullptr);

            auto item = project->getProjectItemForFile (wavFile);
            REQUIRE (item != nullptr);
            CHECK_FALSE (item->hasBeenDeleted());
        }

        cleanup();
    }

    TEST_CASE ("ProjectItem: isForFile")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("isforfile_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        auto wavFile = project->getDefaultDirectory().getChildFile ("test_match.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "test_match", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);

        CHECK (item->isForFile (wavFile));
        CHECK_FALSE (item->isForFile (tempDir.getChildFile ("nonexistent.wav")));

        cleanup();
    }

    TEST_CASE ("ProjectItem: isAbsolutePath")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        auto projectFile = tempDir.getChildFile ("abspath_test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0);
        REQUIRE (sinFile != nullptr);

        // File inside project dir should use relative path
        auto wavFile = project->getDefaultDirectory().getChildFile ("relative_test.wav");
        sinFile->getFile().copyFileTo (wavFile);

        auto item = project->createNewItem (wavFile, ProjectItem::waveItemType(),
                                             "relative_test", {}, ProjectItem::Category::none, false);
        REQUIRE (item != nullptr);
        CHECK_FALSE (item->isAbsolutePath());

        // File outside project dir should use absolute path
        auto externalFile = tempDir.getParentDirectory().getChildFile ("external_test.wav");
        sinFile->getFile().copyFileTo (externalFile);

        auto extItem = project->createNewItem (externalFile, ProjectItem::waveItemType(),
                                                "external_test", {}, ProjectItem::Category::none, false);
        REQUIRE (extItem != nullptr);
        CHECK (extItem->isAbsolutePath());

        externalFile.deleteFile();
        cleanup();
    }

    TEST_CASE ("Project: copied file-based project resolves source files")
    {
        // When a file-based project directory is copied to a new location,
        // opening the copy should resolve project item source files relative
        // to the new location, not the original.
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();

        auto cleanup = [&tempDir]
        {
            tempDir.deleteRecursively (false);
        };

        // 1. Create original project with an audio file
        auto origDir = tempDir.getChildFile ("original");
        origDir.createDirectory();

        auto projectFile = origDir.getChildFile ("test.tracktion");
        ProjectManager::TempProject tp (pm, projectFile, true);
        auto project = tp.project;
        REQUIRE (project != nullptr);

        // Create a wav file inside the project directory
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        REQUIRE (sinFile != nullptr);

        auto importedDir = origDir.getChildFile ("Imported");
        importedDir.createDirectory();
        auto audioInProject = importedDir.getChildFile ("test_audio.wav");
        sinFile->getFile().copyFileTo (audioInProject);
        REQUIRE (audioInProject.existsAsFile());

        // Add the audio file as a project item
        auto audioItem = project->createNewItem (audioInProject, ProjectItem::waveItemType(),
                                                  "test_audio", {}, ProjectItem::Category::imported, false);
        REQUIRE (audioItem != nullptr);

        // Verify original project resolves correctly
        auto origSourceFile = audioItem->getSourceFile();
        CHECK (origSourceFile.existsAsFile());
        CHECK (origSourceFile == audioInProject);

        project->save();

        // 2. Copy the project directory to a new location
        auto copyDir = tempDir.getChildFile ("copy");
        CHECK (origDir.copyDirectoryTo (copyDir));

        auto copiedProjectFile = copyDir.getChildFile ("test.tracktion");
        REQUIRE (copiedProjectFile.existsAsFile());

        auto copiedAudioFile = copyDir.getChildFile ("Imported").getChildFile ("test_audio.wav");
        REQUIRE (copiedAudioFile.existsAsFile());

        // 3. Open the copied project
        ProjectManager::TempProject tp2 (pm, copiedProjectFile, false);
        auto copiedProject = tp2.project;
        REQUIRE (copiedProject != nullptr);

        // 4. Verify the copied project's items resolve to the COPIED location
        for (int i = 0; i < copiedProject->getNumProjectItems(); ++i)
        {
            if (auto item = copiedProject->getProjectItemAt (i))
            {
                if (item->getType() == ProjectItem::waveItemType())
                {
                    auto resolvedFile = item->getSourceFile();
                    CHECK_MESSAGE (resolvedFile.existsAsFile(),
                                   ("Source file should exist: " + resolvedFile.getFullPathName()).toStdString());
                    CHECK_MESSAGE (resolvedFile.isAChildOf (copyDir),
                                   ("Source file should be inside copied dir, got: " + resolvedFile.getFullPathName()).toStdString());
                    CHECK_MESSAGE (! resolvedFile.isAChildOf (origDir),
                                   ("Source file should NOT point to original dir, got: " + resolvedFile.getFullPathName()).toStdString());
                }
            }
        }

        cleanup();
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PROJECT
