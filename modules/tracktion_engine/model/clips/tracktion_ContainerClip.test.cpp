/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../utilities/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

namespace
{
    inline void containerClipExpectPeak (const test_utilities::BufferAndSampleRate& data, TimeRange tr, float expectedPeak)
    {
        const auto sampleRange = toSamples (tr, data.sampleRate);
        const auto peak = data.buffer.getMagnitude (static_cast<int> (sampleRange.getStart()),
                                                    static_cast<int> (sampleRange.getLength()));
        CHECK_MESSAGE (juce::isWithin (peak, expectedPeak, 0.001f),
                       (juce::String ("Expected peak: ") + juce::String (expectedPeak, 4) + ", actual peak: " + juce::String (peak, 4)).toStdString());
    }

    inline void containerClipExpectRMS (const test_utilities::BufferAndSampleRate& data, TimeRange tr, int channel, float expectedRMS)
    {
        const auto sampleRange = toSamples (tr, data.sampleRate);
        const auto rms = data.buffer.getRMSLevel (channel,
                                                   static_cast<int> (sampleRange.getStart()),
                                                   static_cast<int> (sampleRange.getLength()));
        CHECK (std::abs (rms - expectedRMS) <= 0.01f);
    }
}

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("ContainerClip: Basic")
{
    using namespace tracktion::graph::test_utilities;
    using namespace tracktion::engine::test_utilities;

    auto& engine = *Engine::getEngines()[0];
    Clipboard clipboard;
    auto edit = createTestEdit (engine);
    auto audioTrack = getAudioTracks (*edit)[0];
    auto squareFile = getSquareFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 220.0f);

    // Adding child clip
    auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
    CHECK (cc != nullptr);
    auto clip1 = insertWaveClip (*cc, {}, squareFile->getFile(), {{ 1_tp, 2_tp }}, DeleteExistingClips::no);
    clip1->setUsesProxy (false);
    clip1->setAutoTempo (true);

    if constexpr (TimeStretcher::defaultMode != TimeStretcher::soundtouchBetter)
    {
        // Rendered length
        auto res = renderToAudioBuffer (*edit);
        containerClipExpectRMS (res, { 0_tp, 1_tp }, 0, 0.0f);
        containerClipExpectRMS (res, { 1_tp, 2_tp }, 0, 1.0f);
        containerClipExpectRMS (res, { 2_tp, 3_tp }, 0, 0.0f);
    }
}

TEST_CASE ("ContainerClip")
{
    using namespace tracktion::graph::test_utilities;
    using namespace tracktion::engine::test_utilities;

    auto& engine = *Engine::getEngines()[0];
    Clipboard clipboard;
    auto edit = createTestEdit (engine);
    auto audioTrack = getAudioTracks (*edit)[0];
    audioTrack->getVolumePlugin()->setVolumeDb (juce::Decibels::gainToDecibels (0.5f));
    auto squareFile1 = getSquareFile<juce::WavAudioFormat> (44100.0, 3.0, 1, 220.0f);
    auto squareFile2 = getSquareFile<juce::WavAudioFormat> (44100.0, 2.0, 1, 220.0f);

    // Adding child clips
    auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
    CHECK (cc != nullptr);
    auto clip1 = insertWaveClip (*cc, {}, squareFile1->getFile(), {{ 0_tp, 3_tp }}, DeleteExistingClips::no);
    auto clip2 = insertWaveClip (*cc, {}, squareFile2->getFile(), {{ 2_tp, 4_tp }}, DeleteExistingClips::no);

    clip1->setUsesProxy (false);
    clip2->setUsesProxy (false);

    clip1->setAutoTempo (true);
    clip2->setAutoTempo (true);

    clip1->setTimeStretchMode (TimeStretcher::soundtouchBetter);
    clip2->setTimeStretchMode (TimeStretcher::soundtouchBetter);

    // Clip properties
    {
        CHECK_EQ (cc->getSourceLength(), 4_td);
        CHECK_NE (cc->getHash(), static_cast<HashCode> (0));

        CHECK (clip1 != nullptr);
        CHECK (clip2 != nullptr);

        CHECK (clip1->getParent() == cc);
        CHECK (clip2->getParent() == cc);

        CHECK (findClipForState (*edit, clip1->state) != nullptr);
        CHECK (findClipForState (*edit, clip2->state) != nullptr);

        CHECK (findClipForID (*edit, clip1->itemID) != nullptr);
        CHECK (findClipForID (*edit, clip2->itemID) != nullptr);

        CHECK (cc->getClips().contains (clip1.get()));
        CHECK (cc->getClips().contains (clip2.get()));

        CHECK_EQ (getClipsOfTypeRecursive<Clip> (*audioTrack).size(), 3);
        CHECK (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (cc));
        CHECK (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (clip1.get()));
        CHECK (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (clip2.get()));

        CHECK_EQ (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).size(), 2);
        CHECK (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).contains (clip1.get()));
        CHECK (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).contains (clip2.get()));

        CHECK (containsClip (*edit, clip1.get()));
        CHECK (containsClip (*edit, clip2.get()));

        CHECK (getClipsOfType<WaveAudioClip> (*cc).contains (clip1.get()));
        CHECK (getClipsOfType<WaveAudioClip> (*cc).contains (clip2.get()));

        const auto exportables = Exportable::addAllExportables (*edit);
        CHECK (exportables.contains (cc));
        CHECK (exportables.contains (clip1.get()));
        CHECK (exportables.contains (clip2.get()));

        // Plugins
        {
            auto ccVolPlug = dynamic_cast<VolumeAndPanPlugin*> (cc->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());
            auto clip1VolPlug = dynamic_cast<VolumeAndPanPlugin*> (clip1->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());
            auto clip2VolPlug = dynamic_cast<VolumeAndPanPlugin*> (clip2->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());

            auto autoItems = getAllAutomatableEditItems (*edit);
            CHECK (std::find (autoItems.begin(), autoItems.end(), ccVolPlug) != autoItems.end());
            CHECK (std::find (autoItems.begin(), autoItems.end(), clip1VolPlug) != autoItems.end());
            CHECK (std::find (autoItems.begin(), autoItems.end(), clip2VolPlug) != autoItems.end());

            auto allParams = edit->getAllAutomatableParams (true);
            CHECK (allParams.contains (ccVolPlug->volParam.get()));
            CHECK (allParams.contains (clip1VolPlug->volParam.get()));
            CHECK (allParams.contains (clip2VolPlug->volParam.get()));
        }
    }

    // Multiple child clips
    {
        auto res = renderToAudioBuffer (*edit);
        containerClipExpectPeak (res, { 0_tp, 1_tp }, 0.5f);
        containerClipExpectPeak (res, { 1_tp, 2_tp }, 0.5f);
        containerClipExpectPeak (res, { 2_tp, 3_tp }, 1.0f);
        containerClipExpectPeak (res, { 3_tp, 4_tp }, 0.5f);
        containerClipExpectPeak (res, { 4_tp, 5_tp }, 0.0f);
    }

    // Container clip plugins
    {
        // Set the volume of the contained clips to 0.5
        clip1->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (-6.0f);
        clip2->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (-6.0f);

        // Then the volume of the parent clip to 2.0
        cc->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (6.0f);

        auto res = renderToAudioBuffer (*edit);

        containerClipExpectPeak (res, { 0_tp, 1_tp }, 0.5f);
        containerClipExpectPeak (res, { 1_tp, 2_tp }, 0.5f);
        containerClipExpectPeak (res, { 2_tp, 3_tp }, 1.0f);
        containerClipExpectPeak (res, { 3_tp, 4_tp }, 0.5f);
        containerClipExpectPeak (res, { 4_tp, 5_tp }, 0.0f);
    }

    // Clip at non-zero time
    {
        // Move container clip along by 10s
        cc->setStart (10s, false, true);
        CHECK_EQ (cc->getPosition().getEnd(), 15_tp);

        auto res = renderToAudioBuffer (*edit);
        containerClipExpectPeak (res, { 10_tp, 11_tp }, 0.5f);
        containerClipExpectPeak (res, { 11_tp, 12_tp }, 0.5f);
        containerClipExpectPeak (res, { 12_tp, 13_tp }, 1.0f);
        containerClipExpectPeak (res, { 13_tp, 14_tp }, 0.5f);
        containerClipExpectPeak (res, { 14_tp, 15_tp }, 0.0f);
    }

    // Clip offset
    {
        // Set container clip offset of 1s
        cc->setOffset (1s);
        CHECK (cc->getPosition().time == TimeRange (10_tp, 15_tp));

        auto res = renderToAudioBuffer (*edit);
        containerClipExpectPeak (res, { 9_tp, 10_tp }, 0.0f);
        containerClipExpectPeak (res, { 10_tp, 11_tp }, 0.5f);
        containerClipExpectPeak (res, { 11_tp, 12_tp }, 1.0f);
        containerClipExpectPeak (res, { 12_tp, 13_tp }, 0.5f);
        containerClipExpectPeak (res, { 13_tp, 14_tp }, 0.0f);
    }

    // Looping
    {
        cc->setLoopRange ({ 0_tp, 5_tp });
        cc->setLength (10_td, true);
        CHECK_EQ (cc->getPosition().getEnd(), 20_tp);

        auto res = renderToAudioBuffer (*edit);

        // First loop (clipped by offset)
        containerClipExpectPeak (res, { 9_tp, 10_tp }, 0.0f);
        containerClipExpectPeak (res, { 10_tp, 11_tp }, 0.5f);
        containerClipExpectPeak (res, { 11_tp, 12_tp }, 1.0f);
        containerClipExpectPeak (res, { 12_tp, 13_tp }, 0.5f);
        containerClipExpectPeak (res, { 13_tp, 14_tp }, 0.0f);

        // Second loop
        containerClipExpectPeak (res, { 14_tp, 15_tp }, 0.5f);
        containerClipExpectPeak (res, { 15_tp, 16_tp }, 0.5f);
        containerClipExpectPeak (res, { 16_tp, 17_tp }, 1.0f);
        containerClipExpectPeak (res, { 17_tp, 18_tp }, 0.5f);
        containerClipExpectPeak (res, { 18_tp, 19_tp }, 0.0f);

        // Third loop
        containerClipExpectPeak (res, { 19_tp, 20_tp }, 0.5f);
    }

    // Cross-fading contained clips
    {
        // If the contained clips faded linearly where they overlap, the overall magnitue should be 0.5
        clip1->setAutoCrossfade (true);
        clip2->setAutoCrossfade (true);

        CHECK_EQ (clip1->getFadeOut(), 1_td);
        CHECK_EQ (clip2->getFadeIn(), 1_td);

        auto res = renderToAudioBuffer (*edit);

        containerClipExpectPeak (res, { 0_tp, 20_tp }, 0.5f);

        // First loop (clipped by offset)
        containerClipExpectPeak (res, { 9_tp, 10_tp }, 0.0f);
        containerClipExpectPeak (res, { 10_tp, 11_tp }, 0.5f);
        containerClipExpectPeak (res, { 11_tp, 12_tp }, 0.5f);
        containerClipExpectPeak (res, { 12_tp, 13_tp }, 0.5f);
        containerClipExpectPeak (res, { 13_tp, 14_tp }, 0.0f);

        // Second loop
        containerClipExpectPeak (res, { 14_tp, 15_tp }, 0.5f);
        containerClipExpectPeak (res, { 15_tp, 16_tp }, 0.5f);
        containerClipExpectPeak (res, { 16_tp, 17_tp }, 0.5f);
        containerClipExpectPeak (res, { 17_tp, 18_tp }, 0.5f);
        containerClipExpectPeak (res, { 18_tp, 19_tp }, 0.0f);

        // Third loop
        containerClipExpectPeak (res, { 19_tp, 20_tp }, 0.5f);
    }

    // Edit remapping
    {
        // Remove the fades as they aren't remapped
        clip1->setAutoCrossfade (false);
        clip2->setAutoCrossfade (false);

        // Auto tempo so the loops are remapped
        cc->setAutoTempo (true);
        cc->setLoopRange (cc->getLoopRange()); // This converts the looping to beat based but I'm not sure this should have to happen...
        CHECK (cc->beatBasedLooping());

        // Set the tempo to 120, all the times should be halved
        auto& ts = edit->tempoSequence;

        CHECK_EQ (ts.getTempos()[0]->getBpm(), 60.0);
        CHECK (cc->getSyncType() == Clip::syncBarsBeats);
        CHECK (clip1->getSyncType() == Clip::syncBarsBeats);
        CHECK (clip2->getSyncType() == Clip::syncBarsBeats);

        // Test with double tempo
        ts.getTempos()[0]->setBpm (120.0);
        CHECK_EQ (ts.getTempos()[0]->getBpm(), 120.0);

        CHECK_EQ (cc->getPosition().getStart(), 5_tp);
        CHECK_EQ (cc->getPosition().getEnd(), 10_tp);
        CHECK_EQ (cc->getPosition().offset, 0.5_td);

        CHECK_EQ (clip1->getPosition().getStart(), 0_tp);
        CHECK_EQ (clip1->getPosition().getEnd(), 1.5_tp);

        CHECK_EQ (clip2->getPosition().getStart(), 1_tp);
        CHECK_EQ (clip2->getPosition().getEnd(), 2_tp);

        CHECK_EQ (cc->getLoopRange().getLength(), 2.5_td);

        {
            auto res = renderToAudioBuffer (*edit);

            // First loop (clipped by offset)
            containerClipExpectPeak (res, { 4.5_tp, 5_tp }, 0.0f);
            containerClipExpectPeak (res, { 5_tp, 5.5_tp }, 0.5f);
            containerClipExpectPeak (res, { 5.5_tp, 6_tp }, 1.0f);
            containerClipExpectPeak (res, { 6_tp, 6.5_tp }, 0.5f);
            containerClipExpectPeak (res, { 6.5_tp, 7_tp }, 0.0f);

            // Second loop
            containerClipExpectPeak (res, { 7_tp, 7.5_tp }, 0.5f);
            containerClipExpectPeak (res, { 7.5_tp, 8_tp }, 0.5f);
            containerClipExpectPeak (res, { 8_tp, 8.5_tp }, 1.0f);
            containerClipExpectPeak (res, { 8.5_tp, 9_tp }, 0.5f);
            containerClipExpectPeak (res, { 9_tp, 9.5_tp }, 0.0f);

            // Third loop
            containerClipExpectPeak (res, { 9.5_tp, 10_tp }, 0.5f);
        }

        // Test with half original tempo
        ts.getTempos()[0]->setBpm (30.0);
        CHECK_EQ (ts.getTempos()[0]->getBpm(), 30.0);

        CHECK_EQ (cc->getPosition().getStart(), 20_tp);
        CHECK_EQ (cc->getPosition().getEnd(), 40_tp);
        CHECK_EQ (cc->getPosition().offset, 2_td);

        CHECK_EQ (clip1->getPosition().getStart(), 0_tp);
        CHECK_EQ (clip1->getPosition().getEnd(), 6_tp);

        CHECK_EQ (clip2->getPosition().getStart(), 4_tp);
        CHECK_EQ (clip2->getPosition().getEnd(), 8_tp);

        CHECK_EQ (cc->getLoopRange().getLength(), 10_td);

        {
            auto res = renderToAudioBuffer (*edit);

            // First loop (clipped by offset)
            containerClipExpectPeak (res, { 18_tp, 20_tp }, 0.0f);
            containerClipExpectPeak (res, { 20_tp, 22_tp }, 0.5f);
            containerClipExpectPeak (res, { 22_tp, 24_tp }, 1.0f);
            containerClipExpectPeak (res, { 24_tp, 26_tp }, 0.5f);
            containerClipExpectPeak (res, { 26_tp, 28_tp }, 0.0f);

            // Second loop
            containerClipExpectPeak (res, { 28_tp, 30_tp }, 0.5f);
            containerClipExpectPeak (res, { 30_tp, 32_tp }, 0.5f);
            containerClipExpectPeak (res, { 32_tp, 34_tp }, 1.0f);
            containerClipExpectPeak (res, { 34_tp, 36_tp }, 0.5f);
            containerClipExpectPeak (res, { 36_tp, 38_tp }, 0.0f);

            // Third loop
            containerClipExpectPeak (res, { 38_tp, 40_tp }, 0.5f);
        }
    }

    // Container clip fade
    {
        edit->tempoSequence.getTempos()[0]->setBpm (60.0);

        // Just a single sin wave at 0.5
        cc->setLoopRange ({ 0_tp, 1_tp });
        cc->setLength (5s, true);
        cc->setStart (0s, false, true);

        cc->setFadeIn (2s);
        cc->setFadeOut (2s);

        auto res = renderToAudioBuffer (*edit);

        // We're only looping the first clip now so full peak in the middle will be 0.5
        containerClipExpectPeak (res, { 0_tp, 1_tp }, 0.25f);
        containerClipExpectPeak (res, { 2_tp, 4_tp }, 0.5f);
        containerClipExpectPeak (res, { 4_tp, 5_tp }, 0.25f);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS_CLIPS


//==============================================================================
//==============================================================================
#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_CONTAINERCLIP
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../playback/graph/tracktion_BenchmarkUtilities.h"

namespace tracktion::inline engine {

//==============================================================================
//==============================================================================
class ContainerClipBenchmarks   : public juce::UnitTest
{
public:
    ContainerClipBenchmarks()
        : juce::UnitTest ("ContainerClip", "tracktion_benchmarks")
    {}

    void runTest() override
    {
        runCreateLoopedContainerClipBenchmark();
    }

private:
    BenchmarkDescription getDescription (std::string bmName)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = bmName;

        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
            bmCategory, bmName, bmDescription };
    }

    void runCreateLoopedContainerClipBenchmark()
    {
        //- Create a container clip
        //- Create contained clip sequence like a standard drum pattern (4 beats)
        //- Set the container clip's loop range to [0, 4)
        //- Loop the container clip for 48hrs
        //- Benchmark how long this takes to build with EditNodeBuilder

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto& ts = edit->tempoSequence;

        auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*getAudioTracks (*edit)[0], TrackItem::Type::container,
                                                               ts.toTime ({ 0_bp, 4_bp })));
        cc->setLoopRangeBeats ({ 0_bp, 4_bp });
        cc->setEnd (Edit::getMaximumEditEnd(), true);

        // BEAT:    0    1    2    3
        // kick:    x--- x--- x--- x---
        // snare:   x--- ---- x--- ----
        // hat1:    x-x- x-x- x-x- x-x-
        // hat2:    -x-x -x-x -x-x -x-x
        // tom1:    ---- x-x- ---- x-x-

        // Use a real file so the Nodes don't get optomised away
        constexpr double sampleRate = 96'000.0;
        constexpr int bufferSize = 128;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 1.0);

        auto insertClip = [cc, f = sinFile->getFile(), &ts] (BeatRange r)
                          {
                              auto wac = insertWaveClip (*cc, {}, f, { ts.toTime (r) }, DeleteExistingClips::no);
                              wac->setAutoTempo (true);
                              wac->setUsesProxy (false);
                          };

        // Kick
        {
            insertClip ({ 0_bp, 0.25_bd });
            insertClip ({ 1_bp, 0.25_bd });
            insertClip ({ 2_bp, 0.25_bd });
            insertClip ({ 3_bp, 0.25_bd });
        }

        // Snare
        {
            insertClip ({ 0_bp, 0.25_bd });
            insertClip ({ 2_bp, 0.25_bd });
        }

        // Hat 1
        {
            insertClip ({ 0_bp,   0.25_bd });
            insertClip ({ 0.5_bp, 0.25_bd });
            insertClip ({ 1_bp,   0.25_bd });
            insertClip ({ 1.5_bp, 0.25_bd });
            insertClip ({ 2_bp,   0.25_bd });
            insertClip ({ 2.5_bp, 0.25_bd });
            insertClip ({ 3_bp,   0.25_bd });
            insertClip ({ 3.5_bp, 0.25_bd });
        }

        // Hat 2
        {
            insertClip ({ 0.25_bp, 0.25_bd });
            insertClip ({ 0.75_bp, 0.25_bd });
            insertClip ({ 1.25_bp, 0.25_bd });
            insertClip ({ 1.75_bp, 0.25_bd });
            insertClip ({ 2.25_bp, 0.25_bd });
            insertClip ({ 2.75_bp, 0.25_bd });
            insertClip ({ 3.25_bp, 0.25_bd });
            insertClip ({ 3.75_bp, 0.25_bd });
        }

        // Tom
        {
            insertClip ({ 1_bp,   0.25_bd });
            insertClip ({ 1.5_bp, 0.25_bd });
            insertClip ({ 3_bp,   0.25_bd });
            insertClip ({ 3.5_bp, 0.25_bd });
        }

        {
            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, ts };
            CreateNodeParams cnp { processState, sampleRate, bufferSize };

            const ScopedBenchmark sb (createBenchmarkDescription (
                "Node",
                juce::String ("Looping container clip").toStdString(),
                juce::String ("Conatiner clip with 4 beat drum loop over max Edit length").toStdString()));
            [[ maybe_unused ]] auto editNode = createNodeForEdit (*edit, cnp);
        }
    }
};

static ContainerClipBenchmarks containerClipBenchmarks;

} // namespace tracktion::inline engine

#endif
