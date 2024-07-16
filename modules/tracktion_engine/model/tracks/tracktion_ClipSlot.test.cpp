/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPSLOT

 #include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class ClipSlotTests : public juce::UnitTest
{
public:
    ClipSlotTests()
        : juce::UnitTest ("ClipSlot", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];
        runBasicClipSlotTests (engine);
    }

private:
    void runBasicClipSlotTests (Engine& engine)
    {
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 2, 220.0f);

        auto edit = test_utilities::createTestEdit (engine, 1);
        auto track = getAudioTracks (*edit)[0];
        auto& clipSlots = track->getClipSlotList();
        auto& sceneList = edit->getSceneList();

        beginTest ("Basic ClipSlot");
        {
            expectEquals (clipSlots.getClipSlots().size(), 0);

            clipSlots.ensureNumberOfSlots (1);
            expectEquals (clipSlots.getClipSlots().size(), 1);

            auto clipSlot = clipSlots.getClipSlots()[0];
            expect (findClipSlotForID (*edit, clipSlot->itemID) != nullptr);
            auto wac = insertWaveClip (*clipSlot, {}, sinFile->getFile(), {}, DeleteExistingClips::no);
            expect (wac != nullptr);

            expect (clipSlot->getClip() == wac.get());
            expect (clipSlot->getClip()->getSourceFileReference().getFile() == sinFile->getFile());
        }

        beginTest ("Scenes");
        {
            sceneList.ensureNumberOfScenes (clipSlots.getClipSlots().size());
            expectEquals (sceneList.getNumScenes(), 1);
        }

        beginTest ("New track");
        {
            auto newTrack = edit->insertNewAudioTrack (TrackInsertPoint ({}), nullptr);
            expectEquals (newTrack->getClipSlotList().getClipSlots().size(), sceneList.getNumScenes());
        }

        beginTest ("Delete all tracks");
        {
            for (auto at : getAudioTracks (*edit))
                edit->deleteTrack (at);

            auto newTrack = edit->insertNewAudioTrack (TrackInsertPoint ({}), nullptr);
            expectEquals (sceneList.getNumScenes(), 1);
            expectEquals (newTrack->getClipSlotList().getClipSlots().size(), edit->getSceneList().getNumScenes());
        }
    }
};

static ClipSlotTests clipSlotTests;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPSLOT
