/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPS

#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../utilities/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class ContainerClipTests  : public juce::UnitTest
{
public:
    ContainerClipTests()
        : juce::UnitTest ("ContainerClip", "tracktion_engine")
    {}

    void runTest() override
    {
        runBasicContainerClipTests();
        runContainerClipTests();
    }

private:
    void runBasicContainerClipTests()
    {
        using namespace tracktion::graph::test_utilities;
        using namespace tracktion::engine::test_utilities;

        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = createTestEdit (engine);
        auto audioTrack = getAudioTracks (*edit)[0];
        auto squareFile = getSquareFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 220.0f);

        beginTest ("Adding child clip");
        auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
        expect (cc != nullptr);
        auto clip1 = insertWaveClip (*cc, {}, squareFile->getFile(), {{ 1_tp, 2_tp }}, DeleteExistingClips::no);
        clip1->setUsesProxy (false);
        clip1->setAutoTempo (true);

        if constexpr (TimeStretcher::defaultMode != TimeStretcher::soundtouchBetter)
        {
            beginTest ("Rendered length");
            {
                auto res = renderToAudioBuffer (*edit);
                expectRMS (*this, res, { 0_tp, 1_tp }, 0, 0.0f);
                expectRMS (*this, res, { 1_tp, 2_tp }, 0, 1.0f);
                expectRMS (*this, res, { 2_tp, 3_tp }, 0, 0.0f);
            }
        }
    }

    void runContainerClipTests()
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

        beginTest ("Adding child clips");
        auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
        expect (cc != nullptr);
        auto clip1 = insertWaveClip (*cc, {}, squareFile1->getFile(), {{ 0_tp, 3_tp }}, DeleteExistingClips::no);
        auto clip2 = insertWaveClip (*cc, {}, squareFile2->getFile(), {{ 2_tp, 4_tp }}, DeleteExistingClips::no);

        clip1->setUsesProxy (false);
        clip2->setUsesProxy (false);

        clip1->setAutoTempo (true);
        clip2->setAutoTempo (true);

        beginTest ("Clip properties");
        {
            expectEquals (cc->getSourceLength(), 4_td);
            expectNotEquals (cc->getHash(), static_cast<HashCode> (0));

            expect (clip1 != nullptr);
            expect (clip2 != nullptr);

            expect (clip1->getParent() == cc);
            expect (clip2->getParent() == cc);

            expect (findClipForState (*edit, clip1->state) != nullptr);
            expect (findClipForState (*edit, clip2->state) != nullptr);

            expect (findClipForID (*edit, clip1->itemID) != nullptr);
            expect (findClipForID (*edit, clip2->itemID) != nullptr);

            expect (cc->getClips().contains (clip1.get()));
            expect (cc->getClips().contains (clip2.get()));

            expectEquals (getClipsOfTypeRecursive<Clip> (*audioTrack).size(), 3);
            expect (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (cc));
            expect (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (clip1.get()));
            expect (getClipsOfTypeRecursive<Clip> (*audioTrack).contains (clip2.get()));

            expectEquals (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).size(), 2);
            expect (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).contains (clip1.get()));
            expect (getClipsOfTypeRecursive<WaveAudioClip> (*audioTrack).contains (clip2.get()));

            expect (containsClip (*edit, clip1.get()));
            expect (containsClip (*edit, clip2.get()));

            expect (getClipsOfType<WaveAudioClip> (*cc).contains (clip1.get()));
            expect (getClipsOfType<WaveAudioClip> (*cc).contains (clip2.get()));

            const auto exportables = Exportable::addAllExportables (*edit);
            expect (exportables.contains (cc));
            expect (exportables.contains (clip1.get()));
            expect (exportables.contains (clip2.get()));

            // Plugins
            {
                auto ccVolPlug = dynamic_cast<VolumeAndPanPlugin*> (cc->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());
                auto clip1VolPlug = dynamic_cast<VolumeAndPanPlugin*> (clip1->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());
                auto clip2VolPlug = dynamic_cast<VolumeAndPanPlugin*> (clip2->getPluginList()->insertPlugin (VolumeAndPanPlugin::create(), 0).get());

                auto autoItems = getAllAutomatableEditItems (*edit);
                expect (autoItems.contains (ccVolPlug));
                expect (autoItems.contains (clip1VolPlug));
                expect (autoItems.contains (clip2VolPlug));

                auto allParams = edit->getAllAutomatableParams (true);
                expect (allParams.contains (ccVolPlug->volParam.get()));
                expect (allParams.contains (clip1VolPlug->volParam.get()));
                expect (allParams.contains (clip2VolPlug->volParam.get()));
            }
        }

        beginTest ("Multiple child clips");
        {
            auto res = renderToAudioBuffer (*edit);
            expectPeak (*this, res, { 0_tp, 1_tp }, 0.5f);
            expectPeak (*this, res, { 1_tp, 2_tp }, 0.5f);
            expectPeak (*this, res, { 2_tp, 3_tp }, 1.0f);
            expectPeak (*this, res, { 3_tp, 4_tp }, 0.5f);
            expectPeak (*this, res, { 4_tp, 5_tp }, 0.0f);
        }

        beginTest ("Container clip plugins");
        {
            // Set the volume of the contained clips to 0.5
            clip1->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (-6.0f);
            clip2->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (-6.0f);

            // Then the volume of the parent clip to 2.0
            cc->getPluginList()->findFirstPluginOfType<VolumeAndPanPlugin>()->setVolumeDb (6.0f);

            auto res = renderToAudioBuffer (*edit);

            expectPeak (*this, res, { 0_tp, 1_tp }, 0.5f);
            expectPeak (*this, res, { 1_tp, 2_tp }, 0.5f);
            expectPeak (*this, res, { 2_tp, 3_tp }, 1.0f);
            expectPeak (*this, res, { 3_tp, 4_tp }, 0.5f);
            expectPeak (*this, res, { 4_tp, 5_tp }, 0.0f);
        }

        beginTest ("Clip at non-zero time");
        {
            // Move container clip along by 10s
            cc->setStart (10s, false, true);
            expectEquals (cc->getPosition().getEnd(), 15_tp);

            auto res = renderToAudioBuffer (*edit);
            expectPeak (*this, res, { 10_tp, 11_tp }, 0.5f);
            expectPeak (*this, res, { 11_tp, 12_tp }, 0.5f);
            expectPeak (*this, res, { 12_tp, 13_tp }, 1.0f);
            expectPeak (*this, res, { 13_tp, 14_tp }, 0.5f);
            expectPeak (*this, res, { 14_tp, 15_tp }, 0.0f);
        }

        beginTest ("Clip offset");
        {
            // Set container clip offset of 1s
            cc->setOffset (1s);
            expect (cc->getPosition().time == TimeRange (10_tp, 15_tp));

            auto res = renderToAudioBuffer (*edit);
            expectPeak (*this, res, { 9_tp, 10_tp }, 0.0f);
            expectPeak (*this, res, { 10_tp, 11_tp }, 0.5f);
            expectPeak (*this, res, { 11_tp, 12_tp }, 1.0f);
            expectPeak (*this, res, { 12_tp, 13_tp }, 0.5f);
            expectPeak (*this, res, { 13_tp, 14_tp }, 0.0f);
        }

        beginTest ("Looping");
        {
            cc->setLoopRange ({ 0_tp, 5_tp });
            cc->setLength (10_td, true);
            expectEquals (cc->getPosition().getEnd(), 20_tp);

            auto res = renderToAudioBuffer (*edit);

            // First loop (clipped by offset)
            expectPeak (*this, res, { 9_tp, 10_tp }, 0.0f);
            expectPeak (*this, res, { 10_tp, 11_tp }, 0.5f);
            expectPeak (*this, res, { 11_tp, 12_tp }, 1.0f);
            expectPeak (*this, res, { 12_tp, 13_tp }, 0.5f);
            expectPeak (*this, res, { 13_tp, 14_tp }, 0.0f);

            // Second loop
            expectPeak (*this, res, { 14_tp, 15_tp }, 0.5f);
            expectPeak (*this, res, { 15_tp, 16_tp }, 0.5f);
            expectPeak (*this, res, { 16_tp, 17_tp }, 1.0f);
            expectPeak (*this, res, { 17_tp, 18_tp }, 0.5f);
            expectPeak (*this, res, { 18_tp, 19_tp }, 0.0f);

            // Third loop
            expectPeak (*this, res, { 19_tp, 20_tp }, 0.5f);
        }

        beginTest ("Cross-fading contained clips");
        {
            // If the contained clips faded linearly where they overlap, the overall magnitue should be 0.5
            clip1->setAutoCrossfade (true);
            clip2->setAutoCrossfade (true);

            expectEquals (clip1->getFadeOut(), 1_td);
            expectEquals (clip2->getFadeIn(), 1_td);

            auto res = renderToAudioBuffer (*edit);

            expectPeak (*this, res, { 0_tp, 20_tp }, 0.5f);

            // First loop (clipped by offset)
            expectPeak (*this, res, { 9_tp, 10_tp }, 0.0f);
            expectPeak (*this, res, { 10_tp, 11_tp }, 0.5f);
            expectPeak (*this, res, { 11_tp, 12_tp }, 0.5f);
            expectPeak (*this, res, { 12_tp, 13_tp }, 0.5f);
            expectPeak (*this, res, { 13_tp, 14_tp }, 0.0f);

            // Second loop
            expectPeak (*this, res, { 14_tp, 15_tp }, 0.5f);
            expectPeak (*this, res, { 15_tp, 16_tp }, 0.5f);
            expectPeak (*this, res, { 16_tp, 17_tp }, 0.5f);
            expectPeak (*this, res, { 17_tp, 18_tp }, 0.5f);
            expectPeak (*this, res, { 18_tp, 19_tp }, 0.0f);

            // Third loop
            expectPeak (*this, res, { 19_tp, 20_tp }, 0.5f);
        }

        beginTest ("Edit remapping");
        {
            // Remove the fades as they aren't remapped
            clip1->setAutoCrossfade (false);
            clip2->setAutoCrossfade (false);

            // Auto tempo so the loops are remapped
            cc->setAutoTempo (true);
            cc->setLoopRange (cc->getLoopRange()); // This converts the looping to beat based but I'm not sure this should have to happen...
            expect (cc->beatBasedLooping());

            // Set the tempo to 120, all the times should be halved
            auto& ts = edit->tempoSequence;

            expectEquals (ts.getTempos()[0]->getBpm(), 60.0);
            expect (cc->getSyncType() == Clip::syncBarsBeats);
            expect (clip1->getSyncType() == Clip::syncBarsBeats);
            expect (clip2->getSyncType() == Clip::syncBarsBeats);

            // Test with double tempo
            ts.getTempos()[0]->setBpm (120.0);
            expectEquals (ts.getTempos()[0]->getBpm(), 120.0);

            expectEquals (cc->getPosition().getStart(), 5_tp);
            expectEquals (cc->getPosition().getEnd(), 10_tp);
            expectEquals (cc->getPosition().offset, 0.5_td);

            expectEquals (clip1->getPosition().getStart(), 0_tp);
            expectEquals (clip1->getPosition().getEnd(), 1.5_tp);

            expectEquals (clip2->getPosition().getStart(), 1_tp);
            expectEquals (clip2->getPosition().getEnd(), 2_tp);

            expectEquals (cc->getLoopRange().getLength(), 2.5_td);

            {
                auto res = renderToAudioBuffer (*edit);

                // First loop (clipped by offset)
                expectPeak (*this, res, { 4.5_tp, 5_tp }, 0.0f);
                expectPeak (*this, res, { 5_tp, 5.5_tp }, 0.5f);
                expectPeak (*this, res, { 5.5_tp, 6_tp }, 1.0f);
                expectPeak (*this, res, { 6_tp, 6.5_tp }, 0.5f);
                expectPeak (*this, res, { 6.5_tp, 7_tp }, 0.0f);

                // Second loop
                expectPeak (*this, res, { 7_tp, 7.5_tp }, 0.5f);
                expectPeak (*this, res, { 7.5_tp, 8_tp }, 0.5f);
                expectPeak (*this, res, { 8_tp, 8.5_tp }, 1.0f);
                expectPeak (*this, res, { 8.5_tp, 9_tp }, 0.5f);
                expectPeak (*this, res, { 9_tp, 9.5_tp }, 0.0f);

                // Third loop
                expectPeak (*this, res, { 9.5_tp, 10_tp }, 0.5f);
            }

            // Test with half original tempo
            ts.getTempos()[0]->setBpm (30.0);
            expectEquals (ts.getTempos()[0]->getBpm(), 30.0);

            expectEquals (cc->getPosition().getStart(), 20_tp);
            expectEquals (cc->getPosition().getEnd(), 40_tp);
            expectEquals (cc->getPosition().offset, 2_td);

            expectEquals (clip1->getPosition().getStart(), 0_tp);
            expectEquals (clip1->getPosition().getEnd(), 6_tp);

            expectEquals (clip2->getPosition().getStart(), 4_tp);
            expectEquals (clip2->getPosition().getEnd(), 8_tp);

            expectEquals (cc->getLoopRange().getLength(), 10_td);

            {
                auto res = renderToAudioBuffer (*edit);

                // First loop (clipped by offset)
                expectPeak (*this, res, { 18_tp, 20_tp }, 0.0f);
                expectPeak (*this, res, { 20_tp, 22_tp }, 0.5f);
                expectPeak (*this, res, { 22_tp, 24_tp }, 1.0f);
                expectPeak (*this, res, { 24_tp, 26_tp }, 0.5f);
                expectPeak (*this, res, { 26_tp, 28_tp }, 0.0f);

                // Second loop
                expectPeak (*this, res, { 28_tp, 30_tp }, 0.5f);
                expectPeak (*this, res, { 30_tp, 32_tp }, 0.5f);
                expectPeak (*this, res, { 32_tp, 34_tp }, 1.0f);
                expectPeak (*this, res, { 34_tp, 36_tp }, 0.5f);
                expectPeak (*this, res, { 36_tp, 38_tp }, 0.0f);

                // Third loop
                expectPeak (*this, res, { 38_tp, 40_tp }, 0.5f);
            }
        }

        beginTest ("Container clip fade");
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
            expectPeak (*this, res, { 0_tp, 1_tp }, 0.25f);
            expectPeak (*this, res, { 2_tp, 4_tp }, 0.5f);
            expectPeak (*this, res, { 4_tp, 5_tp }, 0.25f);
        }
    }
};

static ContainerClipTests containerClipTests;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_UNIT_TESTS_CLIPS


//==============================================================================
//==============================================================================
#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_CONTAINERCLIP
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../playback/graph/tracktion_BenchmarkUtilities.h"

namespace tracktion { inline namespace engine
{

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

}} // namespace tracktion { inline namespace engine

#endif
