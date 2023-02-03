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
        runContainerClipTests();
    }

private:
    void runContainerClipTests()
    {
        using namespace tracktion::graph::test_utilities;
        using namespace tracktion::engine::test_utilities;

        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = createTestEdit (engine);
        auto audioTrack = getAudioTracks (*edit)[0];
        audioTrack->getVolumePlugin()->setVolumeDb (juce::Decibels::gainToDecibels (0.5f));
        auto sinFile1 = getSinFile<juce::WavAudioFormat> (44100.0, 3.0, 1, 220.0f);
        auto sinFile2 = getSinFile<juce::WavAudioFormat> (44100.0, 2.0, 1, 220.0f);

        beginTest ("Child clips");
        auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
        expect (cc != nullptr);
        auto clip1 = insertWaveClip (*cc, {}, sinFile1->getFile(), {{ 0_tp, 3_tp }}, false);
        auto clip2 = insertWaveClip (*cc, {}, sinFile2->getFile(), {{ 2_tp, 4_tp }}, false);

        // Use WaveNodeRealTime for one of the clips so we're testing both
        clip2->setUsesProxy (false);

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

        beginTest ("Multiple child clips");
        {
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

        // Adjusting tempo of Edit, check child clips also update
    }
};

static ContainerClipTests containerClipTests;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_UNIT_TESTS_CLIPS
