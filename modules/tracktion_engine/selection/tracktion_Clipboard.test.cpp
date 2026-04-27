/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPBOARD

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

namespace
{
    /** Creates a 1-second sine WAV at the given path (mirrors getSinFile pattern). */
    bool createTestWavFile (const juce::File& dest)
    {
        dest.getParentDirectory().createDirectory();
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        assert (sinFile != nullptr);

        return sinFile->getFile().copyFileTo (dest);
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Clipboard: source path resolution during copy/paste")
    {
        auto& engine = *Engine::getEngines()[0];
        auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

        auto projectDir = homeDir.getChildFile ("ClipboardTest_Project");
        auto editFile = projectDir.getChildFile ("edit.tracktionedit");
        auto audioFile = projectDir.getChildFile ("imported/test_audio.wav");
        auto takeFile = projectDir.getChildFile ("imported/test_take2.wav");
        REQUIRE (createTestWavFile (audioFile));
        REQUIRE (createTestWavFile (takeFile));

        auto edit = createEmptyEdit (engine, editFile);
        editFile.create();
        edit->ensureNumberOfAudioTracks (1);
        auto track = getAudioTracks (*edit)[0];

        SUBCASE ("copy resolves relative source to absolute")
        {
            auto clip = insertWaveClip (*track, {}, audioFile,
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);

            // Verify the clip has a relative source path
            auto storedSource = clip->getSourceFileReference().source.get();
            CHECK (! juce::File::isAbsolutePath (storedSource));

            // Copy the clip via the clipboard
            SelectableList selected;
            selected.add (clip.get());
            Clipboard::Clips clipboardClips;
            clipboardClips.addSelectedClips (selected, Edit::getMaximumEditTimeRange(),
                                             Clipboard::Clips::AutomationLocked::no);

            // Verify clipboard state has absolute source path
            REQUIRE (clipboardClips.clips.size() == 1);
            auto clipboardSource = clipboardClips.clips[0].state[IDs::source].toString();
            CHECK (juce::File::isAbsolutePath (clipboardSource));
            CHECK (juce::File (clipboardSource) == audioFile);
        }

        SUBCASE ("round-trip preserves path for same edit")
        {
            auto clip = insertWaveClip (*track, {}, audioFile,
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);

            // Copy the clip (resolves to absolute in clipboard)
            SelectableList selected;
            selected.add (clip.get());
            Clipboard::Clips clipboardClips;
            clipboardClips.addSelectedClips (selected, Edit::getMaximumEditTimeRange(),
                                             Clipboard::Clips::AutomationLocked::no);

            REQUIRE (clipboardClips.clips.size() == 1);
            auto absoluteSource = clipboardClips.clips[0].state[IDs::source].toString();
            REQUIRE (juce::File::isAbsolutePath (absoluteSource));

            // Simulate paste re-relativization for the same edit
            auto reRelativized = SourceFileReference::findPathFromFile (*edit, juce::File (absoluteSource), true);

            // It should resolve back to the same file
            auto resolvedFile = SourceFileReference::findFileFromString (*edit, reRelativized);
            CHECK (resolvedFile == audioFile);
        }

        SUBCASE ("takes (child ValueTrees) source paths are also resolved")
        {
            auto clip = insertWaveClip (*track, {}, audioFile,
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);

            // Add a child ValueTree with a source property to simulate a take
            auto takeState = juce::ValueTree ("TAKE");
            auto relativeTakePath = takeFile.getRelativePathFrom (editFile);
            takeState.setProperty (IDs::source, relativeTakePath, nullptr);
            clip->state.addChild (takeState, -1, nullptr);

            // Copy the clip
            SelectableList selected;
            selected.add (clip.get());
            Clipboard::Clips clipboardClips;
            clipboardClips.addSelectedClips (selected, Edit::getMaximumEditTimeRange(),
                                             Clipboard::Clips::AutomationLocked::no);

            REQUIRE (clipboardClips.clips.size() == 1);

            // Check the child's source was resolved to absolute
            auto& clipboardState = clipboardClips.clips[0].state;
            bool foundTake = false;

            for (int i = 0; i < clipboardState.getNumChildren(); ++i)
            {
                auto child = clipboardState.getChild (i);

                if (child.hasType ("TAKE") && child.hasProperty (IDs::source))
                {
                    auto childSource = child[IDs::source].toString();
                    CHECK (juce::File::isAbsolutePath (childSource));
                    CHECK (juce::File (childSource) == takeFile);
                    foundTake = true;
                }
            }

            CHECK (foundTake);
        }

        SUBCASE ("ProjectItemID source is not modified during copy")
        {
            auto clipState = juce::ValueTree (IDs::AUDIOCLIP);
            clipState.setProperty (IDs::source, "1a2b3c4d/5e6f7g8h", nullptr);
            clipState.setProperty (IDs::start, 0.0, nullptr);
            clipState.setProperty (IDs::length, 1.0, nullptr);

            Clipboard::Clips clipboardClips;
            clipboardClips.addClip (0, clipState);

            REQUIRE (clipboardClips.clips.size() == 1);
            auto clipboardSource = clipboardClips.clips[0].state[IDs::source].toString();
            CHECK (clipboardSource == "1a2b3c4d/5e6f7g8h");
        }

        // Cleanup
        edit.reset();
        projectDir.deleteRecursively (false);
    }

    TEST_CASE ("Clipboard: pasteIntoEdit creates a clip from an external path-based ref with no ProjectItem")
    {
        // Reproduces the drag-and-drop "Don't Copy" path: an external file's
        // ProjectItemRef goes through the paste flow without any corresponding
        // ProjectItem registered in the ProjectManager.
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();
        auto cleanup = [&tempDir] { tempDir.deleteRecursively (false); };

        // External wav file (no project context)
        auto externalWav = tempDir.getChildFile ("external_audio.wav");
        REQUIRE (createTestWavFile (externalWav));
        REQUIRE (externalWav.existsAsFile());

        auto edit = test_utilities::createTestEdit (engine);
        REQUIRE (edit != nullptr);

        // Inner scope so all Ptrs into the Edit are destroyed before edit.reset().
        {
            auto track = getAudioTracks (*edit)[0];
            REQUIRE (track != nullptr);

            // Build a Clipboard::ProjectItems with a path-based ref, without ever
            // calling createNewItem — this is exactly what the fixed import flow
            // ends up doing once the transient ProjectItem has been destroyed.
            Clipboard::ProjectItems content;
            content.itemIDs.push_back ({ ProjectItemRef::fromAbsolutePath (externalWav), {} });

            // Sanity: confirm the ref does NOT resolve to a ProjectItem
            REQUIRE (pm.getProjectItem (content.itemIDs[0].itemID) == nullptr);
            REQUIRE (content.itemIDs[0].itemID.isAbsolutePath());
            REQUIRE (track->getClips().isEmpty());

            EditInsertPoint insertPoint (*edit);
            insertPoint.setNextInsertPoint (0_tp, juce::ReferenceCountedObjectPtr<Track> (track));

            Clipboard::ContentType::EditPastingOptions opts (*edit, insertPoint);
            opts.silent = true;          // keep paste synchronous
            opts.startTrack = track;
            opts.startTime = 0_tp;

            CHECK (content.pasteIntoEdit (opts));

            // The track should now contain a single WaveAudioClip referencing the external file
            auto clips = track->getClips();
            REQUIRE (clips.size() == 1);

            auto wave = dynamic_cast<WaveAudioClip*> (clips[0]);
            REQUIRE (wave != nullptr);

            CHECK (wave->getOriginalFile() == externalWav);
            CHECK (wave->getPosition().getStart() == 0_tp);
            CHECK (wave->getPosition().getLength().inSeconds() > 0.0);
        }

        edit.reset();
        cleanup();
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPBOARD
