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
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPBOARD
