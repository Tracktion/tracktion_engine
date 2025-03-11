/*
,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUTOMATION_CURVE_LIST

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    TEST_CASE("AutomationCurveList")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        auto edit = test_utilities::createTestEdit (engine);

        // Sin file must outlive everything!
        auto fileLength = 5_td;
        auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        AudioTrack::Ptr track;
        WaveAudioClip::Ptr clip;

        {
            track = getAudioTracks (*edit)[0];
            clip = insertWaveClip (*track, {}, squareFile->getFile(), { .time = { 0_tp, fileLength } },
                                   DeleteExistingClips::no);
            CHECK(clip);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 5_tp }, 0)
                    == doctest::Approx (1.0f));
        }

        // Construct curve
        {
            CHECK(! clip->getAutomationCurveList (false));

            auto curveList = clip->getAutomationCurveList (true);
            CHECK(curveList);

            auto volPlugin = track->getVolumePlugin();
            auto volParam = volPlugin->volParam;
            volPlugin->setSliderPos (0.0f);

            CHECK(! volParam->isAutomationActive());
            auto curveMod = curveList->addCurve (*volParam);
            CHECK(curveMod);
            CHECK_EQ(curveList->getItems().size(), 1);

            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
            curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
            CHECK(volParam->isAutomationActive());

            volParam->updateStream(); //ddd shouldn't need this!

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // First 2s full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f)); // Last 2s no vol
        }

        // Move clip to 1s
        {
            clip->setStart (1s, false, true);

            auto volParam = track->getVolumePlugin()->volParam;
            volParam->updateStream(); //ddd shouldn't need this!

            // Curve should move with clip
            auto tempSourceRender = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 1_tp }, 0)
                    == doctest::Approx (0.0f)); // No clip, should use default automation value
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 1_tp, 3_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // Full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f)); // No vol
        }

        // Delete and reload the Edit and check final state again
        {
            auto pid = edit->getProjectItemID();
            auto editState = edit->state;
            track.reset();
            clip.reset();
            edit.reset();

            edit = Edit::createEdit ({ engine, editState, pid });
            track = getAudioTracks (*edit)[0];
            clip = dynamic_cast<WaveAudioClip*> (track->getClips()[0]);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 1_tp }, 0)
                   == doctest::Approx (0.0f)); // No clip, should use default automation value
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 1_tp, 3_tp }, 0)
                   == doctest::Approx (1.0f).epsilon (0.01f)); // Full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                   == doctest::Approx (0.0f)); // No vol
        }

        // Remove the curve
        {
            auto volParam = track->getVolumePlugin()->volParam;

            auto curveList = clip->getAutomationCurveList (false);
            CHECK(curveList);
            CHECK_EQ(curveList->getItems().size(), 1);

            curveList->getItems()[0]->remove();
            CHECK_EQ(curveList->getItems().size(), 0);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                   == doctest::Approx (0.0f)); // No vol
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUTOMATION_CURVE_LIST