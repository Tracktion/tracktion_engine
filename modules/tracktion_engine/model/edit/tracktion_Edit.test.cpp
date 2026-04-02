/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDIT

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

//==============================================================================
//==============================================================================
TEST_SUITE("tracktion_engine")
{
    TEST_CASE("Testing Edit defaults")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);

        CHECK(edit);
        CHECK(getAudioTracks (*edit).size() == 1);
    }

    TEST_CASE ("insertSpaceIntoEdit: moves master volume automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto masterVol = edit->getMasterVolumePlugin();
        REQUIRE (masterVol != nullptr);

        auto volParam = masterVol->volParam.get();
        REQUIRE (volParam != nullptr);

        auto& curve = volParam->getCurve();
        curve.addPoint (5_tp, 0.5f, 0.0f, um);

        CHECK (curve.getPointTime (0) == 5_tp);

        insertSpaceIntoEdit (*edit, { 2_tp, 2_td });

        // Point should move from 5s to 7s
        CHECK (curve.getPointTime (0) == 7_tp);
    }

    TEST_CASE ("insertSpaceIntoEdit: moves global macro parameter automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto& globalMacros = edit->getGlobalMacros();
        auto macroParam = globalMacros.getMacroParameterListForWriting().createMacroParameter();
        REQUIRE (macroParam != nullptr);

        auto& curve = macroParam->getCurve();
        curve.addPoint (5_tp, 0.5f, 0.0f, um);

        CHECK (curve.getPointTime (0) == 5_tp);

        insertSpaceIntoEdit (*edit, { 2_tp, 2_td });

        // Point should move from 5s to 7s
        CHECK (curve.getPointTime (0) == 7_tp);
    }

    TEST_CASE ("insertSpaceIntoEdit: moves rack plugin automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto rackType = edit->getRackList().addNewRack();
        auto volumePlugin = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
        rackType->addPlugin (volumePlugin, {}, true);

        auto volAndPan = dynamic_cast<VolumeAndPanPlugin*> (volumePlugin.get());
        REQUIRE (volAndPan != nullptr);

        auto volParam = volAndPan->volParam.get();
        auto& curve = volParam->getCurve();
        curve.addPoint (5_tp, 0.5f, 0.0f, um);

        CHECK (curve.getPointTime (0) == 5_tp);

        insertSpaceIntoEdit (*edit, { 2_tp, 2_td });

        // Point should move from 5s to 7s
        CHECK (curve.getPointTime (0) == 7_tp);
    }

    TEST_CASE ("deleteRegionOfTracks: removes and shifts master volume automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto masterVol = edit->getMasterVolumePlugin();
        REQUIRE (masterVol != nullptr);

        auto volParam = masterVol->volParam.get();
        REQUIRE (volParam != nullptr);

        auto& curve = volParam->getCurve();
        // Add points: point within deleted region will be removed, point after will shift
        curve.addPoint (2_tp, 0.5f, 0.0f, um);
        curve.addPoint (10_tp, 0.5f, 0.0f, um);

        CHECK (curve.getNumPoints() == 2);

        // Delete region from 3s to 6s
        deleteRegionOfTracks (*edit, { 3_tp, 6_tp }, false, CloseGap::yes, nullptr);

        CHECK (curve.getNumPoints() == 2);
        CHECK (curve.getPointTime (0) == 2_tp);
        // Point at 10s should shift back by 3s to 7s
        CHECK (curve.getPointTime (1) == 7_tp);
    }

    TEST_CASE ("deleteRegionOfTracks: removes and shifts global macro automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto& globalMacros = edit->getGlobalMacros();
        auto macroParam = globalMacros.getMacroParameterListForWriting().createMacroParameter();
        REQUIRE (macroParam != nullptr);

        auto& curve = macroParam->getCurve();
        // Use same values to avoid boundary preservation points being added
        curve.addPoint (2_tp, 0.5f, 0.0f, um);
        curve.addPoint (10_tp, 0.5f, 0.0f, um);

        CHECK (curve.getNumPoints() == 2);

        // Delete region from 3s to 6s
        deleteRegionOfTracks (*edit, { 3_tp, 6_tp }, false, CloseGap::yes, nullptr);

        CHECK (curve.getNumPoints() == 2);
        CHECK (curve.getPointTime (0) == 2_tp);
        CHECK (curve.getPointTime (1) == 7_tp);
    }

    TEST_CASE ("deleteRegionOfTracks: removes and shifts rack plugin automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto rackType = edit->getRackList().addNewRack();
        auto volumePlugin = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
        rackType->addPlugin (volumePlugin, {}, true);

        auto volAndPan = dynamic_cast<VolumeAndPanPlugin*> (volumePlugin.get());
        REQUIRE (volAndPan != nullptr);

        auto volParam = volAndPan->volParam.get();
        auto& curve = volParam->getCurve();
        // Use same values to avoid boundary preservation points being added
        curve.addPoint (2_tp, 0.5f, 0.0f, um);
        curve.addPoint (10_tp, 0.5f, 0.0f, um);

        CHECK (curve.getNumPoints() == 2);

        deleteRegionOfTracks (*edit, { 3_tp, 6_tp }, false, CloseGap::yes, nullptr);

        CHECK (curve.getNumPoints() == 2);
        CHECK (curve.getPointTime (0) == 2_tp);
        CHECK (curve.getPointTime (1) == 7_tp);
    }

    TEST_CASE ("deleteRegionOfTracks: shifts pitch sequence")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);

        auto& pitchSeq = edit->pitchSequence;

        // Add pitch changes at various positions
        pitchSeq.insertPitch (2_tp);
        pitchSeq.insertPitch (5_tp);
        pitchSeq.insertPitch (10_tp);

        const int numPitchesBefore = pitchSeq.getNumPitches();
        CHECK (numPitchesBefore == 4); // Including initial pitch at 0

        // Delete region from 3s to 6s
        deleteRegionOfTracks (*edit, { 3_tp, 6_tp }, false, CloseGap::yes, nullptr);

        // Pitch at 5s should be removed, pitch at 10s should shift back
        CHECK (pitchSeq.getNumPitches() == 3);
    }

    TEST_CASE ("deleteRegionOfTracks: CloseGap::no does not shift automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        auto um = &edit->getUndoManager();

        auto masterVol = edit->getMasterVolumePlugin();
        REQUIRE (masterVol != nullptr);

        auto volParam = masterVol->volParam.get();
        auto& curve = volParam->getCurve();
        curve.addPoint (2_tp, 0.3f, 0.0f, um);
        curve.addPoint (10_tp, 0.7f, 0.0f, um);

        CHECK (curve.getNumPoints() == 2);

        // Delete region but don't close gap
        deleteRegionOfTracks (*edit, { 3_tp, 6_tp }, false, CloseGap::no, nullptr);

        // Points should remain unchanged
        CHECK (curve.getNumPoints() == 2);
        CHECK (curve.getPointTime (0) == 2_tp);
        CHECK (curve.getPointTime (1) == 10_tp);
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Edit: File preview Edit")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        auto editToMatch = Edit::createSingleTrackEdit (engine);
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat>
                        (44100.0, 10.0, 2, 220.0f);
        bool couldMatchTempo = false;

        auto edit = Edit::createEditForPreviewingFile (engine, sinFile->getFile(), editToMatch.get(),
                                                       true, true, &couldMatchTempo, {});
        CHECK (edit != nullptr);
        CHECK (! couldMatchTempo);
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_EDITITEMID

namespace tracktion::inline engine {

//==============================================================================
//==============================================================================
class EditItemIDBenchmarks  : public juce::UnitTest
{
public:
    EditItemIDBenchmarks()
        : juce::UnitTest ("EditItemID", "tracktion_benchmarks")
    {}

    void runTest() override
    {
        // Create an empty edit
        // Add a MIDI clip with some random data
        // Copy/paste that clip 49 times on the same track (50 clips)
        // Copy/paste that track 99 times (5000 clips)
        // Copy/paste all clips (10,000 clips)
        // Load edit again from tree and see how long it takes to load

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit (engine);

        beginTest ("Benchmark: Copy/paste");

        {
            auto c = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0.0s, TimePosition (1.0s) }, nullptr);
            auto t1 = c->getTrack();

            ScopedBenchmark sb (getDescription ("Copy/paste clip 49 times"));

            for (int i = 1; i < 50; ++i)
            {
                auto clipState = c->state.createCopy();
                EditItemID::remapIDs (clipState, nullptr, c->edit);

               #if JUCE_DEBUG
                auto newClipID = EditItemID::fromID (clipState);
                jassert (newClipID != EditItemID::fromID (c->state));
                jassert (findClipForID (c->edit, newClipID) == nullptr);
                jassert (edit->clipCache.findItem (newClipID) == nullptr);

                jassert (! t1->state.getChildWithProperty (IDs::id, newClipID).isValid());
               #endif

                t1->state.appendChild (clipState, c->getUndoManager());

               #if JUCE_DEBUG
                jassert (t1->findClipForID (newClipID) != nullptr);
               #endif
            }
        }

        {
            auto t1 = getAudioTracks (*edit)[0];
            jassert (t1->getNumTrackItems() == 50);
            auto preceeding = t1->state;

            ScopedBenchmark sb (getDescription ("Copy/paste track 99 times"));

            for (int i = 1; i < 100; ++i)
            {
                auto trackState = t1->state.createCopy();
                EditItemID::remapIDs (trackState, nullptr, *edit);

               #if JUCE_DEBUG
                auto newTrackID = EditItemID::fromID (trackState);
                jassert (newTrackID != EditItemID::fromID (t1->state));
                jassert (findTrackForID (*edit, newTrackID) == nullptr);
                jassert (edit->trackCache.findItem (newTrackID) == nullptr);

                jassert (! t1->state.getChildWithProperty (IDs::id, newTrackID).isValid());
               #endif

                edit->insertTrack (trackState, {}, preceeding, nullptr);
                preceeding = trackState;

               #if JUCE_DEBUG
                jassert (findTrackForID (*edit, newTrackID) != nullptr);
               #endif
            }
        }

        {
            auto allAudioTracks = getAudioTracks (*edit);
            jassert (allAudioTracks.size() == 100);

            ScopedBenchmark sb (getDescription ("Copy/paste all 5,000 clips using Clipboard"));
            Clipboard::Clips content;
            int trackOffset = 0;

            for (auto at : allAudioTracks)
            {
                for (auto c : at->getClips())
                    if (auto mc = dynamic_cast<MidiClip*> (c))
                        content.addClip (trackOffset, mc->state);

                ++trackOffset;
            }

            EditInsertPoint insertPoint (*edit);
            content.pasteIntoEdit (*edit, insertPoint, nullptr);

           #if JUCE_DEBUG
            const int numClips = std::accumulate (allAudioTracks.begin(), allAudioTracks.end(), 0,
                                                  [] (int total, auto t) { return total + t->getNumTrackItems(); });
            jassert (numClips == 10'000);
           #endif
        }

        {
            auto editStateCopy = edit->state.createCopy();

            ScopedBenchmark sb (getDescription ("Load Edit from state"));
            Edit editCopy ({ engine, editStateCopy, ProjectItemID::createNewID (ProjectID{}) });
            jassert (getAudioTracks (editCopy).size() == 100);
        }
    }

private:
    BenchmarkDescription getDescription (std::string bmName)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = bmName;

        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
                 bmCategory, bmName, bmDescription };
    }
};

static EditItemIDBenchmarks editItemIDBenchmarks;

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS
