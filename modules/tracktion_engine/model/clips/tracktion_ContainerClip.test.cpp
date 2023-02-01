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

        // Create an empty edit
        // Create a container clip
        // Add two audio clips
        // Render output and expect content contains the two clips with audio level double when overlapped

        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit (engine);
        auto audioTrack = getAudioTracks (*edit)[0];
        auto sinFile = getSinFile<juce::WavAudioFormat> (44100.0, 2.0);

        beginTest ("Container Clip");
        {
            auto cc = dynamic_cast<ContainerClip*> (insertNewClip (*audioTrack, TrackItem::Type::container, { 0_tp, 5_tp }));
            expect (cc != nullptr);
            auto clip1 = insertWaveClip (*cc, {}, sinFile->getFile(), {{ 1_tp, 3_tp }}, false);
            auto clip2 = insertWaveClip (*cc, {}, sinFile->getFile(), {{ 2_tp, 4_tp }}, false);

            expectEquals (cc->getSourceLength(), 4_td);
            expectNotEquals (cc->getHash(), static_cast<HashCode> (0));

            expect (clip1 != nullptr);
            expect (clip2 != nullptr);

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

//ddd            expectPeak (*this, *edit, { 0_tp, 1_tp }, {}, 0.0f);
//            expectPeak (*this, *edit, { 1_tp, 2_tp }, {}, 1.0f);
//            expectPeak (*this, *edit, { 2_tp, 3_tp }, {}, 2.0f);
//            expectPeak (*this, *edit, { 3_tp, 4_tp }, {}, 1.0f);
//            expectPeak (*this, *edit, { 4_tp, 5_tp }, {}, 0.0f);
        }

        // Crossfading of clips
        // Adjusting tempo of Edit, check child clips also update
        // Looping of CollectionClip
    }
};

static ContainerClipTests containerClipTests;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_UNIT_TESTS_CLIPS
