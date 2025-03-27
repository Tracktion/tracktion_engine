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
#include <tracktion_engine/playback/graph/tracktion_BenchmarkUtilities.h>
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
            context->volPlugin->smoothingRampTimeSeconds = 0.0;
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

            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue == 0.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue == 0.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4.99_bp).baseValue == 0.0f);
            CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue);
            CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue);

            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // First 2s full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f)); // Last 2s no vol
        }

        // Move clip to 1s
        {
            context->clip->setStart (1s, false, true);

            auto curveMod = context->clip->getAutomationCurveList (false)->getItems()[0];
            CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue == 1.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue == 0.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue == 0.0f);
            CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5.99_bp).baseValue == 0.0f);
            CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue);
            CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 7_bp).baseValue);

            // Curve should move with clip
            auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 1_tp }, 0)
                    == doctest::Approx (0.0f)); // No clip, should use default automation value
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 1_tp, 3_tp }, 0)
                    == doctest::Approx (1.0f).epsilon (0.01f)); // Full vol
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 4_tp, 6_tp }, 0)
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

        auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 2_tp }, 0)
                == doctest::Approx (1.0f).epsilon (0.001f)); // First full vol

        auto expectedGain = volumeFaderPositionToGain (0.1f * decibelsToVolumeFaderPosition (0.0f));
        CHECK (test_utilities::getRMSLevel (tempSourceRender, { 3_tp, 5_tp }, 0)
                == doctest::Approx (expectedGain).epsilon (0.001f)); // Last 2s 10%
    }

    TEST_CASE("AutomationCurveList: tempo & offset")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);
        auto& ts = context->edit->tempoSequence;
        ts.getTempo (0)->setBpm (120.0);
        CHECK (context->clip->getLengthInBeats().inBeats() == doctest::Approx (10));

        context->clip->setStart (ts.toTime (2_bp), true, false);
        CHECK (context->clip->getOffsetInBeats().inBeats() == doctest::Approx (2));
        CHECK (context->clip->getLengthInBeats().inBeats() == doctest::Approx (8));

        auto curveList = context->clip->getAutomationCurveList (true);
        CHECK(curveList);

        CHECK(! context->volParam->isAutomationActive());
        auto curveMod = curveList->addCurve (*context->volParam);
        CHECK(curveMod);
        CHECK_EQ(curveList->getItems().size(), 1);

        context->volPlugin->setVolumeDb (0.0f);

        auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
        curve.addPoint (5_bp, 1.0f, 0.0, nullptr);
        curve.addPoint (5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 5b
        CHECK(context->volParam->isAutomationActive());

        CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue);
        CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue == 1.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue == 1.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue == 1.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue == 0.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue == 0.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 7_bp).baseValue == 0.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 8_bp).baseValue == 0.0f);
        CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 9_bp).baseValue == 0.0f);
        CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 10_bp).baseValue);
        CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 11_bp).baseValue);

        auto tempSourceRender = test_utilities::renderToAudioBuffer (*context->edit);
        CHECK (test_utilities::getRMSLevel (tempSourceRender, ts.toTime ({ 0_bp, 2_bp }), 0)
                == doctest::Approx (0.0f).epsilon (0.001f)); // First 2b none
        CHECK (test_utilities::getRMSLevel (tempSourceRender, ts.toTime ({ 2_bp, 4.5_bp }), 0)
                == doctest::Approx (1.0f).epsilon (0.001f)); // Up to half full vol
        CHECK (test_utilities::getRMSLevel (tempSourceRender, ts.toTime ({ 5.5_bp, 9_bp }), 0)
                == doctest::Approx (0.0).epsilon (0.001f)); // After half, none
    }

    TEST_CASE("AutomationCurveList: unlinked")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);
        auto& ts = context->edit->tempoSequence;

        auto curveList = context->clip->getAutomationCurveList (true);
        auto curveMod = curveList->addCurve (*context->volParam);

        auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
        curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
        curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b

        auto& timing = curveMod->getCurveTiming (CurveModifierType::absolute);
        timing.unlinked = true;
        CHECK (timing.unlinked.get());

        timing.start = 0_bp;
        timing.length = 0_bd;
        timing.looping = false;
        timing.loopStart = 0_bp;
        timing.loopLength = 0_bd;

        CHECK_EQ (timing.start.get(), 0_bp);
        CHECK_EQ (timing.length.get(), 1_bd);   // Min length applied
        CHECK(! timing.looping.get());
        CHECK_EQ (timing.loopStart.get(), 0_bp);
        CHECK_EQ (timing.loopLength.get(), 1_bd);   // Min loop length applied

        SUBCASE("Unlooped")
        {
            CHECK (! timing.looping.get());
            timing.length = 4_bd;

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 2.5_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp);
                CHECK (! v.modValue);
                CHECK (! v.baseValue);
            }

            timing.start = 2_bp;

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0.5_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp);
                CHECK (! v.modValue);
                CHECK (! v.baseValue);
            }

            context->clip->setStart (1_tp, true, false);

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp);
                CHECK (! v.modValue);
                CHECK (! v.baseValue);
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 1.5_bp);
                CHECK (! v.modValue);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp);
                CHECK (! v.modValue);
                CHECK (! v.baseValue);
            }
        }

        SUBCASE("Looping")
        {
            timing.looping = true;

            timing.loopStart = 1_bp;
            timing.loopLength = 3_bd;

            context->clip->setEnd (toTime (6_bp, ts), false);

            {
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue.value()
                        == doctest::Approx (0.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3.99_bp).baseValue.value()
                        == doctest::Approx (0.0f));

                // End of loop, not inclusive so loops round to start
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5.99_bp).baseValue.value()
                        == doctest::Approx (0.0f));

                // End of clip so no value
                CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue.has_value());
            }

            // Change start position
            timing.start  = 1_bp;

            {
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue.value()
                        == doctest::Approx (0.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2.99_bp).baseValue.value()
                        == doctest::Approx (0.0f));

                // End of loop, not inclusive so loops round to start
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue.value()
                        == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4.99_bp).baseValue.value()
                        == doctest::Approx (0.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue.value()
                        == doctest::Approx (0.0f));

                // End of clip so no value
                CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue.has_value());
            }
        }

        SUBCASE("Init timing from clip properties")
        {
            auto& clip = *context->clip;
            CHECK (context->edit->tempoSequence.getTempoAt (0_tp).bpm.get() == doctest::Approx (60.0f));
            clip.setStart (10_tp, false, true); // Should be ignored
            clip.setOffset (2_td);
            clip.setLength (6_td, true);
            clip.setLoopRangeBeats ({ 1_bp, 4_bd });

            CHECK (clip.getEditBeatRange().getLength() == 6_bd);
            CHECK (clip.getOffsetInBeats() == 2_bd);
            CHECK (clip.isLooping());
            CHECK (clip.getLoopRangeBeats() == BeatRange (1_bp, 4_bd));

            CHECK (timing.unlinked.get());
            timing.unlinked = false;

            CHECK_EQ (timing.start.get(), 2_bp);
            CHECK_EQ (timing.length.get(), 6_bd);
            CHECK(timing.looping.get());
            CHECK_EQ (timing.loopStart.get(), 1_bp);
            CHECK_EQ (timing.loopLength.get(), 4_bd);
        }
    }

    TEST_CASE("AutomationCurveList: linked")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        auto curveList = context->clip->getAutomationCurveList (true);
        auto curveMod = curveList->addCurve (*context->volParam);

        auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
        curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
        curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b

        auto& timing = curveMod->getCurveTiming (CurveModifierType::absolute);
        timing.unlinked = false;
        CHECK (! timing.unlinked.get());

        auto& clip = *context->clip;
        clip.setOffset (2_td);
        clip.setLength (4_td, true);
        clip.setLoopRangeBeats ({});

        SUBCASE("Unlooped")
        {
            CHECK (! clip.isLooping());
            CHECK (clip.getEditBeatRange().getLength() == 4_bd);
            CHECK (clip.getOffsetInBeats() == 2_bd);

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0.5_bp);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp);
                CHECK (! v.baseValue);
            }

            context->clip->setStart (1_tp, false, true);
            CHECK (context->clip->getOffsetInBeats() == 2_bd);
            CHECK (clip.getEditBeatRange().getLength() == 4_bd);

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp);
                CHECK (! v.baseValue);
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 1.49_bp);
                CHECK (v.baseValue.value() == doctest::Approx (1.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 1.5_bp);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 4.99_bp);
                CHECK (v.baseValue.value() == doctest::Approx (0.0f));
            }

            {
                auto v = getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp);
                CHECK (! v.baseValue);
            }
        }

        SUBCASE("Looping")
        {
            clip.setOffset (0_td);
            clip.setLength (6_td, true);
            clip.setLoopRangeBeats ({ 1_bp, 3_bd });

            CHECK (clip.isLooping());
            CHECK (clip.getEditBeatRange().getStart() == 0_bp);
            CHECK (clip.getEditBeatRange().getLength() == 6_bd);
            CHECK (clip.getOffsetInBeats() == 0_bd);

            {
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue.value()
                       == doctest::Approx (0.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3.99_bp).baseValue.value()
                       == doctest::Approx (0.0f));

                // End of loop, not inclusive so loops round to start
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5.99_bp).baseValue.value()
                       == doctest::Approx (0.0f));

                // End of clip so no value
                CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue.has_value());
            }

            // Change start position
            clip.setStart (1_tp, true, true);

            CHECK (clip.isLooping());
            CHECK (clip.getEditBeatRange().getStart() == 1_bp);
            CHECK (clip.getEditBeatRange().getLength() == 6_bd);
            CHECK (clip.getOffsetInBeats() == 1_bd);

            {
                CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 0_bp).baseValue.has_value());

                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 1_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2.49_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 2.5_bp).baseValue.value()
                       == doctest::Approx (0.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 3_bp).baseValue.value()
                       == doctest::Approx (0.0f));

                // End of loop, not inclusive so loops round to start
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 4_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 5.49_bp).baseValue.value()
                       == doctest::Approx (1.0f));
                CHECK (getValuesAtEditPosition (*curveMod, *context->volParam, 6_bp).baseValue.value()
                       == doctest::Approx (0.0f));

                // End of clip so no value
                CHECK (! getValuesAtEditPosition (*curveMod, *context->volParam, 7_bp).baseValue.has_value());
            }
        }
    }

    TEST_CASE("AutomationCurveList: Move/copy clip")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        {
            auto curveList = context->clip->getAutomationCurveList (true);
            auto curveMod = curveList->addCurve (*context->volParam);

            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
            curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
        }

        context->edit->ensureNumberOfAudioTracks (2);
        auto audioTracks = getAudioTracks (*context->edit);
        auto at1 = audioTracks[0];
        auto at2 = audioTracks[1];

        SUBCASE("Move clip to second track")
        {
            context->clip->moveTo (*at2);

            {
                auto volPlug1 = at1->getVolumePlugin();
                auto volParam1 = volPlug1->volParam;
                CHECK (! volPlug1->isAutomationNeeded());
                CHECK (! volParam1->isAutomationActive());
                CHECK (volParam1->getModifiers().isEmpty());
            }

            auto curveList = context->clip->getAutomationCurveList (true);
            CHECK (curveList->getItems().size() == 1);

            auto curveMod = curveList->getItems().front();
            CHECK (curveMod);

            auto& absCurve = curveMod->getCurve (CurveModifierType::absolute).curve;
            CHECK_EQ (absCurve.getNumPoints(), 2);

            auto volPlug2 = at2->getVolumePlugin();
            auto volParam2 = volPlug2->volParam;
            CHECK (volParam2->isAutomationActive());
            CHECK (! volParam2->getModifiers().isEmpty());

            auto& ts = context->edit->tempoSequence;
            volParam2->updateToFollowCurve (ts.toTime (0_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (2_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (4_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (0.0f));
        }

        SUBCASE("Move clip to second track with no corresponding plugin")
        {
            at2->getVolumePlugin()->deleteFromParent();
            context->clip->moveTo (*at2);
            CHECK (context->clip->getAutomationCurveList (true)->getItems().empty());

            {
                auto volPlug1 = at1->getVolumePlugin();
                auto volParam1 = volPlug1->volParam;
                CHECK (! volPlug1->isAutomationNeeded());
                CHECK (! volParam1->isAutomationActive());
                CHECK (volParam1->getModifiers().isEmpty());
            }
        }

        SUBCASE ("Duplicate clip to second track")
        {
            auto stateCopy = context->clip->state.createCopy();
            EditItemID::remapIDs (stateCopy, nullptr, *context->edit);
            auto newClipID = EditItemID::fromID (stateCopy);
            CHECK (newClipID != EditItemID::fromID (context->clip->state));
            CHECK (findClipForID (*context->edit, newClipID) == nullptr);
            CHECK (context->edit->clipCache.findItem (newClipID) == nullptr);

            CHECK (! at2->state.getChildWithProperty (IDs::id, newClipID.toVar()).isValid());
            at2->state.appendChild (stateCopy, getUndoManager_p (*at2));
            auto newClip = at2->findClipForID (newClipID);
            CHECK (newClip);

            {
                // Check track 1 is the same
                auto volPlug1 = at1->getVolumePlugin();
                auto volParam1 = volPlug1->volParam;
                CHECK (volPlug1->isAutomationNeeded());
                CHECK (volParam1->isAutomationActive());
                CHECK (! volParam1->getModifiers().isEmpty());
            }

            auto curveList = context->clip->getAutomationCurveList (true);
            CHECK (curveList->getItems().size() == 1);

            auto curveMod = curveList->getItems().front();
            CHECK (curveMod);

            auto& absCurve = curveMod->getCurve (CurveModifierType::absolute).curve;
            CHECK_EQ (absCurve.getNumPoints(), 2);

            auto volPlug2 = at2->getVolumePlugin();
            auto volParam2 = volPlug2->volParam;
            CHECK (volParam2->isAutomationActive());
            CHECK (! volParam2->getModifiers().isEmpty());

            auto& ts = context->edit->tempoSequence;
            volParam2->updateToFollowCurve (ts.toTime (0_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (2_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (4_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (0.0f));
        }

        SUBCASE ("Duplicate track")
        {
            {
                // Delete the old track 2
                context->edit->deleteTrack (at2);

                // Duplicate track 1 to track 2
                auto stateCopy = context->track->state.createCopy();
                EditItemID::remapIDs (stateCopy, nullptr, *context->edit);
                auto newTrackID = EditItemID::fromID (stateCopy);
                auto newTrack = context->edit->insertTrack (TrackInsertPoint::getEndOfTracks (*context->edit), stateCopy, nullptr);
                CHECK (newTrack->itemID == newTrackID);

                at2 = dynamic_cast<AudioTrack*> (newTrack.get());
                CHECK (at2);
            }

            {
                // Check track 1 is the same
                auto volPlug1 = at1->getVolumePlugin();
                auto volParam1 = volPlug1->volParam;
                CHECK (volPlug1->isAutomationNeeded());
                CHECK (volParam1->isAutomationActive());
                CHECK (! volParam1->getModifiers().isEmpty());
            }

            auto curveList = context->clip->getAutomationCurveList (true);
            CHECK (curveList->getItems().size() == 1);

            auto curveMod = curveList->getItems().front();
            CHECK (curveMod);

            auto& absCurve = curveMod->getCurve (CurveModifierType::absolute).curve;
            CHECK_EQ (absCurve.getNumPoints(), 2);

            auto volPlug2 = at2->getVolumePlugin();
            auto volParam2 = volPlug2->volParam;
            CHECK (volParam2->isAutomationActive());
            CHECK (! volParam2->getModifiers().isEmpty());

            auto& ts = context->edit->tempoSequence;
            volParam2->updateToFollowCurve (ts.toTime (0_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (2_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (4_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (0.0f));
        }
    }

    TEST_CASE("AutomationCurveList: Clipboard")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        {
            auto curveList = context->clip->getAutomationCurveList (true);
            auto curveMod = curveList->addCurve (*context->volParam);

            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
            curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
        }

        context->edit->ensureNumberOfAudioTracks (2);
        auto audioTracks = getAudioTracks (*context->edit);
        auto at1 = audioTracks[0];
        auto at2 = audioTracks[1];

        {
            auto volPlug = at1->getVolumePlugin();
            auto volParam = volPlug->volParam;
            CHECK (volPlug->isAutomationNeeded());
            CHECK (volParam->isAutomationActive());
            CHECK (! volParam->getModifiers().isEmpty());

            auto curveMod = at1->getClips()[0]->getAutomationCurveList (false)->getItems()[0];
            CHECK (getParameter (*curveMod) == volParam);
        }

        SUBCASE("Cut/paste clip to second track")
        {
            {
                Clipboard::Clips content;
                content.addClip (0, context->clip->state);

                context->clip->removeFromParent();

                EditInsertPoint insertPoint (*context->edit);
                insertPoint.setNextInsertPoint (at2, {});
                CHECK(content.pasteIntoEdit (*context->edit, insertPoint, nullptr));

                context->clip = dynamic_cast<WaveAudioClip*> (at2->getClips()[0]);
                CHECK(context->clip);
            }

            {
                auto volPlug1 = at1->getVolumePlugin();
                auto volParam1 = volPlug1->volParam;
                CHECK (! volPlug1->isAutomationNeeded());
                CHECK (! volParam1->isAutomationActive());
                CHECK (volParam1->getModifiers().isEmpty());
            }

            auto curveList = context->clip->getAutomationCurveList (true);
            CHECK (curveList->getItems().size() == 1);

            auto curveMod = curveList->getItems().front();
            CHECK (curveMod);

            auto& absCurve = curveMod->getCurve (CurveModifierType::absolute).curve;
            CHECK_EQ (absCurve.getNumPoints(), 2);

            auto volPlug2 = at2->getVolumePlugin();
            auto volParam2 = volPlug2->volParam;
            CHECK (volParam2->isAutomationActive());
            CHECK (! volParam2->getModifiers().isEmpty());

            auto& ts = context->edit->tempoSequence;
            volParam2->updateToFollowCurve (ts.toTime (0_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (2_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (1.0f));
            volParam2->updateToFollowCurve (ts.toTime (4_bp));
            CHECK (volParam2->getCurrentValue() == doctest::Approx (0.0f));
        }

        SUBCASE("Copy/paste clip to second track")
        {
            {
                Clipboard::Clips content;
                content.addClip (0, context->clip->state);

                EditInsertPoint insertPoint (*context->edit);
                insertPoint.setNextInsertPoint (at2, {});
                CHECK(content.pasteIntoEdit (*context->edit, insertPoint, nullptr));
            }

            for (auto at : { at1, at2 })
            {
                auto volPlug = at->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());

                auto curveMod = at->getClips()[0]->getAutomationCurveList (false)->getItems()[0];
                CHECK (getParameter (*curveMod) == volParam);
            }

            // Delete first clip, vol should be inactive
            {
                context->clip->removeFromParent();

                auto volPlug = at1->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (! volPlug->isAutomationNeeded());
                CHECK (! volParam->isAutomationActive());
                CHECK (volParam->getModifiers().isEmpty());
            }

            // Second vol should still be active
            {
                auto volPlug = at2->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());
            }
        }

        SUBCASE("Copy/paste clip to same track")
        {
            {
                Clipboard::Clips content;
                content.addClip (0, context->clip->state);

                EditInsertPoint insertPoint (*context->edit);
                insertPoint.setNextInsertPoint (at1, context->clip->getEditTimeRange().getEnd());
                CHECK(content.pasteIntoEdit (*context->edit, insertPoint, nullptr));
            }

            {
                auto volPlug = at1->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());

                {
                    auto clips = at1->getClips();
                    auto curveMod1 = clips[0]->getAutomationCurveList (false)->getItems()[0];
                    CHECK (getParameter (*curveMod1) == volParam);
                    auto curveMod2 = clips[1]->getAutomationCurveList (false)->getItems()[0];
                    CHECK (getParameter (*curveMod2) == volParam);
                }
            }

            auto& ts = context->edit->tempoSequence;
            context->volParam->updateToFollowCurve (ts.toTime (0_bp));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (1.0f));
            context->volParam->updateToFollowCurve (ts.toTime (2_bp));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (1.0f));
            context->volParam->updateToFollowCurve (ts.toTime (4_bp));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (0.0f));

            // This should use the second clip's curve
            context->volParam->updateToFollowCurve (ts.toTime (0_bp + 5_bd));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (1.0f));
            context->volParam->updateToFollowCurve (ts.toTime (2_bp + 5_bd));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (1.0f));
            context->volParam->updateToFollowCurve (ts.toTime (4_bp + 5_bd));
            CHECK (context->volParam->getCurrentValue() == doctest::Approx (0.0f));
        }

        SUBCASE("Copy/paste clip to second track with no corresponding plugin")
        {
            at2->getVolumePlugin()->deleteFromParent();

            {
                Clipboard::Clips content;
                content.addClip (0, context->clip->state);

                EditInsertPoint insertPoint (*context->edit);
                insertPoint.setNextInsertPoint (at2, {});
                CHECK(content.pasteIntoEdit (*context->edit, insertPoint, nullptr));

                context->clip = dynamic_cast<WaveAudioClip*> (at2->getClips()[0]);
                CHECK(context->clip);
            }

            CHECK (context->clip->getAutomationCurveList (true)->getItems().empty());

            {
                auto volPlug = at1->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());
            }
        }

        SUBCASE ("Copy/paste track")
        {
            {
                // Delete the old track 2
                context->edit->deleteTrack (at2);

                // Duplicate track 1 to track 2
                Clipboard::Tracks content;
                content.tracks.push_back (at1->state.createCopy());

                EditInsertPoint insertPoint (*context->edit);
                insertPoint.setNextInsertPoint (at1, {});

                content.pasteIntoEdit (*context->edit, insertPoint, nullptr);

                at2 = dynamic_cast<AudioTrack*> (getAudioTracks (*context->edit)[1]);
                CHECK (at2);
                CHECK (at1 != at2);
            }

            for (auto at : { at1, at2 })
            {
                auto volPlug = at->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());

                auto curveMod = at->getClips()[0]->getAutomationCurveList (false)->getItems()[0];
                CHECK (getParameter (*curveMod) == volParam);
            }
        }
    }

    TEST_CASE ("AutomationCurveList: Clip plugin")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto context = AutomationCurveListTestContext::create (engine);

        context->edit->ensureNumberOfAudioTracks (2);
        auto audioTracks = getAudioTracks (*context->edit);
        auto at1 = audioTracks[0];
        auto at2 = audioTracks[1];

        auto checkTrackVolParamsArentActive = [at1, at2]
        {
            for (auto at : { at1, at2 })
            {
                auto volPlug = at->getVolumePlugin();
                auto volParam = volPlug->volParam;
                CHECK (! volPlug->isAutomationNeeded());
                CHECK (! volParam->isAutomationActive());
                CHECK (volParam->getModifiers().isEmpty());
            }
        };

        checkTrackVolParamsArentActive();

        auto plugin = context->clip->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0);
        context->volPlugin = dynamic_cast<VolumeAndPanPlugin*> (plugin.get());
        context->volParam = context->volPlugin->volParam;

        checkTrackVolParamsArentActive();

        {
            auto curveList = context->clip->getAutomationCurveList (true);
            auto curveMod = curveList->addCurve (*context->volParam);

            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
            curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
        }

        checkTrackVolParamsArentActive();

        SUBCASE("Duplicate clip to second track")
        {
            auto stateCopy = context->clip->state.createCopy();
            EditItemID::remapIDs (stateCopy, nullptr, *context->edit);
            auto newClipID = EditItemID::fromID (stateCopy);
            CHECK (newClipID != EditItemID::fromID (context->clip->state));
            CHECK (findClipForID (*context->edit, newClipID) == nullptr);
            CHECK (context->edit->clipCache.findItem (newClipID) == nullptr);

            CHECK (! at2->state.getChildWithProperty (IDs::id, newClipID.toVar()).isValid());
            at2->state.appendChild (stateCopy, getUndoManager_p (*at2));
            auto newClip = at2->findClipForID (newClipID);
            CHECK (newClip);

            // Nothing should be connected to the track plugins
            checkTrackVolParamsArentActive();

            // Clip on track 1 and 2 should have the same properties though
            for (auto clip : { at1->getClips()[0], at2->getClips()[0] })
            {
                auto volPlug = clip->getPluginList()->getPluginsOfType<VolumeAndPanPlugin>()[0];
                auto volParam = volPlug->volParam;
                CHECK (volPlug->isAutomationNeeded());
                CHECK (volParam->isAutomationActive());
                CHECK (! volParam->getModifiers().isEmpty());

                auto curveList = clip->getAutomationCurveList (true);
                CHECK (curveList->getItems().size() == 1);

                auto curveMod = curveList->getItems().front();
                CHECK (curveMod);

                auto& absCurve = curveMod->getCurve (CurveModifierType::absolute).curve;
                CHECK_EQ (absCurve.getNumPoints(), 2);

                auto& ts = context->edit->tempoSequence;
                volParam->updateToFollowCurve (ts.toTime (0_bp));
                CHECK (volParam->getCurrentValue() == doctest::Approx (1.0f));
                volParam->updateToFollowCurve (ts.toTime (2_bp));
                CHECK (volParam->getCurrentValue() == doctest::Approx (1.0f));
                volParam->updateToFollowCurve (ts.toTime (4_bp));
                CHECK (volParam->getCurrentValue() == doctest::Approx (0.0f));
            }
        }
    }

    TEST_CASE ("AutomationCurveList: Save and reload Edit")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        auto checkTrackVolParams = [] (Edit& edit, bool shouldBeActive)
        {
            auto ats = getAudioTracks (edit);
            auto track1VolPlugin = ats[0]->getVolumePlugin();
            auto clipVolPlugin = ats[1]->getClips()[0]->getPluginList()->getPluginsOfType<VolumeAndPanPlugin>()[0];

            for (auto volPlug : { track1VolPlugin, clipVolPlugin })
            {
                auto volParam = volPlug->volParam;
                CHECK_EQ (volPlug->isAutomationNeeded(), shouldBeActive);
                CHECK_EQ (volParam->isAutomationActive(), shouldBeActive);
                CHECK_NE (volParam->getModifiers().isEmpty(), shouldBeActive);
            }
        };

        juce::ValueTree oldEditState;

        // Original Edit
        // Track 1 clip automating track volume
        // Track 2 clip automating clip vol plugin
        {
            auto context = AutomationCurveListTestContext::create (engine);
            context->edit->ensureNumberOfAudioTracks (2);
            auto audioTracks = getAudioTracks (*context->edit);
            auto at2 = audioTracks[1];

            // Configure track 2 first or we'll be copying the track vol plugin assignment as well
            {
                auto stateCopy = context->clip->state.createCopy();
                EditItemID::remapIDs (stateCopy, nullptr, *context->edit);
                auto newClipID = EditItemID::fromID (stateCopy);
                at2->state.appendChild (stateCopy, getUndoManager_p (*at2));
                auto newClip = at2->findClipForID (newClipID);

                auto plugin = newClip->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0);
                auto clipVolPlugin = newClip->getPluginList()->getPluginsOfType<VolumeAndPanPlugin>().getFirst();

                auto curveList = newClip->getAutomationCurveList (true);
                auto curveMod = curveList->addCurve (*clipVolPlugin->volParam);

                auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
                curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
                curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
            }

            // Configure track 1
            {
                auto curveList = context->clip->getAutomationCurveList (true);
                auto curveMod = curveList->addCurve (*context->volParam);
                CHECK (getParameter (*curveMod) == context->volParam);

                auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
                curve.addPoint (2.5_bp, 1.0f, 0.0, nullptr);
                curve.addPoint (2.5_bp, 0.0f, 0.0, nullptr); // Sharp square from 1-0 at 2.5b
            }

            checkTrackVolParams (*context->edit, true);

            {
                auto clip = at2->getClips()[0];
                auto curveList = clip->getAutomationCurveList (false);
                CHECK(curveList->getItems().size() == 1);
                auto curveMod = curveList->getItems()[0];
                auto clipVolPlugin = clip->getPluginList()->getPluginsOfType<VolumeAndPanPlugin>().getFirst();
                CHECK (getParameter (*curveMod).get() == clipVolPlugin->volParam);
            }

            oldEditState = context->edit->state.createCopy();
        }

        // Reload Edit
        {
            auto loadedEdit = benchmark_utilities::loadEditFromValueTree (engine, oldEditState);
            checkTrackVolParams (*loadedEdit, true);

            auto ats = getAudioTracks (*loadedEdit);

            {
                auto clip = ats[0]->getClips()[0];
                auto curveList = clip->getAutomationCurveList (false);
                CHECK(curveList->getItems().size() == 1);
                auto curveMod = curveList->getItems()[0];
                auto trackVolPlugin = ats[0]->pluginList.getPluginsOfType<VolumeAndPanPlugin>().getFirst();
                CHECK (getParameter (*curveMod).get() == trackVolPlugin->volParam);

                curveList->removeCurve (0);
            }

            {
                auto clip = ats[1]->getClips()[0];
                auto curveList = clip->getAutomationCurveList (false);
                CHECK(curveList->getItems().size() == 1);
                auto curveMod = curveList->getItems()[0];
                auto clipVolPlugin = clip->getPluginList()->getPluginsOfType<VolumeAndPanPlugin>().getFirst();
                CHECK (getParameter (*curveMod).get() == clipVolPlugin->volParam);

                curveList->removeCurve (0);
            }

            checkTrackVolParams (*loadedEdit, false);
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUTOMATION_CURVE_LIST