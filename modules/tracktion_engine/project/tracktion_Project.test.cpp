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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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

            // Files outside of the project folder should be absolute
            REQUIRE (! waveClip->getSourceFileReference().isUsingProjectReference());
            REQUIRE (juce::File::isAbsolutePath (waveClip->getSourceFileReference().source.get()));
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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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
            TracktionArchiveFile archive (engine, archiveFile);

            // Add project file
            CHECK (archive.addFile (projectFile, projectFile.getParentDirectory(),
                                    TracktionArchiveFile::CompressionType::none));

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
                                         TracktionArchiveFile::CompressionType::none);
                    }
                }
            }

            archive.flush();
        }

        // Verify archive
        {
            TracktionArchiveFile archive (engine, archiveFile);
            CHECK (archive.isValidArchive());
            CHECK (archive.getNumFiles() >= 2); // At least project file + edit file
        }

        // === Unarchive ===
        auto extractDir = tempDir.getChildFile ("extracted");
        extractDir.createDirectory();

        juce::Array<juce::File> extractedFiles;
        {
            TracktionArchiveFile archive (engine, archiveFile);
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

            CHECK (EditFileOperations (*edit).save (false, true, false));
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
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PROJECT
