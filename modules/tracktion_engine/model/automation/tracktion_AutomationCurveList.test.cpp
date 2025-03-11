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

    struct AutomationCurveListTestContext
    {
        static std::unique_ptr<AutomationCurveListTestContext> create (Engine& engine)
        {
            auto context = std::make_unique<AutomationCurveListTestContext>();

            context->edit = test_utilities::createTestEdit (engine);

            // Sin file must outlive everything!
            auto fileLength = 5_td;
            context->squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());
            context->track = getAudioTracks (*context->edit)[0];
            context->clip = insertWaveClip (*context->track, {}, context->squareFile->getFile(), { .time = { 0_tp, fileLength } },
                                           DeleteExistingClips::no);

            context->volPlugin = context->track->getVolumePlugin();
            context->volParam = context->volPlugin->volParam;
            context->volPlugin->setSliderPos (0.0f);

            return context;
        }

        std::unique_ptr<Edit> edit;
        std::unique_ptr<juce::TemporaryFile> squareFile;
        AudioTrack::Ptr track;
        WaveAudioClip::Ptr clip;

        VolumeAndPanPlugin::Ptr volPlugin;
        AutomatableParameter::Ptr volParam;
    };

TEST_SUITE("tracktion_engine")
{
    TEST_CASE("AutomationCurveList: absolute")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        {
            CHECK(context->clip);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f));
        }

        // Construct curve
        {
            CHECK(! context->clip->getAutomationCurveList (false));

            auto curveList = context->clip->getAutomationCurveList (true);
            CHECK(curveList);

            CHECK(! context->volParam->isAutomationActive());
            auto curveMod = curveList->addCurve (*context->volParam);
            CHECK(curveMod);
            CHECK_EQ(curveList->getItems().size(), 1);

            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
            curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
            CHECK(context->volParam->isAutomationActive());

            context->volParam->updateStream(); //ddd shouldn't need this!

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // First 2s full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f)); // Last 2s no vol
        }

        // Move clip to 1s
        {
            context->clip->setStart (1s, false, true);

            auto volParam = context->track->getVolumePlugin()->volParam;
            volParam->updateStream(); //ddd shouldn't need this!

            // Curve should move with clip
            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 1_tp }, 0)
                    == doctest::Approx (0.0f)); // No clip, should use default automation value
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 1_tp, 3_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // Full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f)); // No vol
        }

        // Delete and reload the Edit and check final state again
        {
            auto pid = context->edit->getProjectItemID();
            auto editState = context->edit->state;
            auto squareFile = std::move (context->squareFile);
            context = std::make_unique<AutomationCurveListTestContext>();

            context->squareFile = std::move (squareFile);
            context->edit = Edit::createEdit ({ engine, editState, pid });
            context->track = getAudioTracks (*context->edit)[0];
            context->clip = dynamic_cast<WaveAudioClip*> (context->track->getClips()[0]);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 1_tp }, 0)
                   == doctest::Approx (0.0f)); // No clip, should use default automation value
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 1_tp, 3_tp }, 0)
                   == doctest::Approx (1.0f).epsilon (0.01f)); // Full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                   == doctest::Approx (0.0f)); // No vol
        }

        // Remove the curve
        {
            auto volParam = context->track->getVolumePlugin()->volParam;

            auto curveList = context->clip->getAutomationCurveList (false);
            CHECK(curveList);
            CHECK_EQ(curveList->getItems().size(), 1);

            curveList->getItems()[0]->remove();
            CHECK_EQ(curveList->getItems().size(), 0);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 5_tp }, 0)
                   == doctest::Approx (0.0f)); // No vol
        }
    }

    TEST_CASE("AutomationCurveList: relative")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        auto curveList = context->clip->getAutomationCurveList (true);
        CHECK(curveList);

        CHECK(! context->volParam->isAutomationActive());
        auto curveMod = curveList->addCurve (*context->volParam);
        CHECK(curveMod);
        CHECK_EQ(curveList->getItems().size(), 1);

        auto& curve = curveMod->getCurve (CurveModifierType::relative).curve;
        curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr);
        curve.addPoint (2.5_bp, 0.25f, 0.0, nullptr); // Sharp square from 0-0.25 at 2.5b
        CHECK(context->volParam->isAutomationActive());

        context->volParam->updateStream(); //ddd shouldn't need this!

        auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                == doctest::Approx (0.0f)); // First no vol

        auto expectedGain = volumeFaderPositionToGain (0.25f);
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                == doctest::Approx (expectedGain).epsilon (0.001f)); // Last 2s +10%
    }

    TEST_CASE("AutomationCurveList: scale")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        auto curveList = context->clip->getAutomationCurveList (true);
        CHECK(curveList);

        CHECK(! context->volParam->isAutomationActive());
        auto curveMod = curveList->addCurve (*context->volParam);
        CHECK(curveMod);
        CHECK_EQ(curveList->getItems().size(), 1);

        context->volPlugin->setVolumeDb (0.0f);

        auto& curve = curveMod->getCurve (CurveModifierType::scale).curve;
        curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
        curve.addPoint (2.5_bp, 0.1f, 0.0, nullptr); // Sharp square from 1-0.1 at 2.5b
        CHECK(context->volParam->isAutomationActive());

        context->volParam->updateStream(); //ddd shouldn't need this!

        auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                == doctest::Approx (1.0f).epsilon (0.001f)); // First full vol

        auto expectedGain = volumeFaderPositionToGain (0.1f * decibelsToVolumeFaderPosition (0.0f));
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                == doctest::Approx (expectedGain).epsilon (0.001f)); // Last 2s 10%
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUTOMATION_CURVE_LIST