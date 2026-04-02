/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPSLOT

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
 #include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ClipSlot")
    {
        auto& engine = *Engine::getEngines()[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 2, 220.0f);

        auto edit = test_utilities::createTestEdit (engine, 1);
        auto track = getAudioTracks (*edit)[0];
        auto& clipSlots = track->getClipSlotList();
        auto& sceneList = edit->getSceneList();

        // Basic ClipSlot
        {
            CHECK_EQ (clipSlots.getClipSlots().size(), 0);

            clipSlots.ensureNumberOfSlots (1);
            CHECK_EQ (clipSlots.getClipSlots().size(), 1);

            auto clipSlot = clipSlots.getClipSlots()[0];
            CHECK (findClipSlotForID (*edit, clipSlot->itemID) != nullptr);
            auto wac = insertWaveClip (*clipSlot, {}, sinFile->getFile(), {}, DeleteExistingClips::no);
            CHECK (wac != nullptr);

            CHECK (clipSlot->getClip() == wac.get());
            CHECK (clipSlot->getClip()->getSourceFileReference().getFile() == sinFile->getFile());
        }

        // Scenes
        {
            sceneList.ensureNumberOfScenes (clipSlots.getClipSlots().size());
            CHECK_EQ (sceneList.getNumScenes(), 1);
        }

        // New track
        {
            auto newTrack = edit->insertNewAudioTrack (TrackInsertPoint ({}), nullptr);
            CHECK_EQ (newTrack->getClipSlotList().getClipSlots().size(), sceneList.getNumScenes());
        }

        // Delete all tracks
        {
            for (auto at : getAudioTracks (*edit))
                edit->deleteTrack (at);

            auto newTrack = edit->insertNewAudioTrack (TrackInsertPoint ({}), nullptr);
            CHECK_EQ (sceneList.getNumScenes(), 1);
            CHECK_EQ (newTrack->getClipSlotList().getClipSlots().size(), edit->getSceneList().getNumScenes());
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPSLOT
