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
            edit->setProjectItemID (editItem1->getID());

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
            edit->setProjectItemID (editItem2->getID());

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
        CHECK (project->getProjectItemForID (editItem1->getID()) != nullptr);
        CHECK (project->getProjectItemForID (editItem2->getID()) != nullptr);

        // Verify the audio file was added as a project item
        auto audioItem = project->getProjectItemForFile (sinFile->getFile());
        CHECK (audioItem != nullptr);

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
            edit->setProjectItemID (editItem->getID());

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

            auto loadedEdit = loadEditForExamining (pm, newEditItem->getID());
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
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PROJECT
