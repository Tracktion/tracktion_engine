/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_SOURCE_FILE_REFERENCE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("SourceFileReference: relative path heuristic")
    {
        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        REQUIRE (sinFile != nullptr);

        // Helper: creates edit at editPath, inserts clip pointing to audioFile,
        // returns whether the stored source path is relative
        auto isStoredPathRelative = [&] (const juce::File& editFile,
                                         const juce::File& audioFile,
                                         bool alwaysRelative = false) -> bool
        {
            auto edit = createEmptyEdit (engine, editFile);
            edit->alwaysUseRelativePaths = alwaysRelative;
            editFile.create();
            edit->ensureNumberOfAudioTracks (1);
            auto track = getAudioTracks (*edit)[0];
            auto clip = insertWaveClip (*track, {}, audioFile,
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            return ! juce::File::isAbsolutePath (clip->getSourceFileReference().source.get());
        };

        auto homeDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

        SUBCASE ("Same user workspace: relative")
        {
            auto editFile = homeDir.getChildFile ("Music/SFRTest_Project/edit.tracktionedit");
            auto audioFile = homeDir.getChildFile ("Music/SFRTest_Samples/kick.wav");
            audioFile.getParentDirectory().createDirectory();
            sinFile->getFile().copyFileTo (audioFile);
            CHECK (isStoredPathRelative (editFile, audioFile));
            audioFile.deleteFile();
            audioFile.getParentDirectory().deleteRecursively (false);
            editFile.deleteFile();
            editFile.getParentDirectory().deleteRecursively (false);
        }

        SUBCASE ("Deeply nested cross-directory: relative")
        {
            auto editFile = homeDir.getChildFile ("SFRTest_stuff/edits/d/e/f/bar.tracktionedit");
            auto audioFile = homeDir.getChildFile ("SFRTest_stuff/audio/b/c/blah.wav");
            audioFile.getParentDirectory().createDirectory();
            sinFile->getFile().copyFileTo (audioFile);
            CHECK (isStoredPathRelative (editFile, audioFile));
            audioFile.deleteFile();
            homeDir.getChildFile ("SFRTest_stuff").deleteRecursively (false);
            editFile.deleteFile();
        }

        SUBCASE ("Temp folder audio: absolute")
        {
            auto editFile = homeDir.getChildFile ("Music/SFRTest_Project/edit.tracktionedit");
            auto tempAudio = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("sfr_test_audio.wav");
            sinFile->getFile().copyFileTo (tempAudio);
            CHECK (! isStoredPathRelative (editFile, tempAudio));
            tempAudio.deleteFile();
            editFile.deleteFile();
        }

        SUBCASE ("alwaysUseRelativePaths forces relative")
        {
            auto editFile = homeDir.getChildFile ("Music/SFRTest_Project/edit.tracktionedit");
            auto tempAudio = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("sfr_test_audio.wav");
            sinFile->getFile().copyFileTo (tempAudio);
            CHECK (isStoredPathRelative (editFile, tempAudio, true));
            tempAudio.deleteFile();
            editFile.deleteFile();
        }

        SUBCASE ("Resolved path still points to original file")
        {
            auto editFile = homeDir.getChildFile ("Music/SFRTest_Project/edit.tracktionedit");
            auto audioDir = homeDir.getChildFile ("Music/SFRTest_Samples");
            audioDir.createDirectory();
            auto audioFile = audioDir.getChildFile ("kick.wav");
            sinFile->getFile().copyFileTo (audioFile);

            auto edit = createEmptyEdit (engine, editFile);
            editFile.create();
            edit->ensureNumberOfAudioTracks (1);
            auto track = getAudioTracks (*edit)[0];
            auto clip = insertWaveClip (*track, {}, audioFile,
                                        { .time = { 0_tp, 1_tp } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            CHECK (clip->getSourceFileReference().getFile() == audioFile);

            audioFile.deleteFile();
            audioDir.deleteRecursively (false);
            editFile.deleteFile();
            editFile.getParentDirectory().deleteRecursively (false);
        }
    }
}

} // namespace tracktion::inline engine

#endif
