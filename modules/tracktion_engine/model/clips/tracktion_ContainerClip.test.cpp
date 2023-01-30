/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../utilities/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//dddinline std::unique_ptr<juce::TemporaryFile> render (Edit& edit)
//{
//    std::unique_ptr<juce::TemporaryFile> destFile;
//    Renderer::renderToFile ({}, destFile.getFile(), edit, { 0s, edit.getLength() }, {});
//
//    return destFile;
//}
//
//inline void expectPeak (juce::UnitTest& ut, Engine& engine, juce::File file, TimeRange tr, float expectedPeak)
//{
//    std::unique_ptr reader = engine.getAudioFileFormatManager().createReaderFor (file);
//    auto stats = logStats (ut, Renderer::measureStatistics ("Tests", edit, tr, toBitSet (tracks), blockSize));
//    ut.expect (juce::isWithin (stats.peak, expectedPeak, 0.001f), juce::String ("Expected peak: ") + juce::String (expectedPeak, 4));
//}

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
            [[ maybe_unused ]] auto cc = audioTrack->insertNewClip (TrackItem::Type::container, { 0_tp, 5_tp }, nullptr);
//ddd            cc->insertWaveClip ({}, sinFile, { 1s, 3s }, false);
//            cc->insertWaveClip ({}, sinFile, { 2s, 4s }, false);
//
//            expectPeak (*this, *edit, { 0_tp, 1_tp }, {}, 0.0f);
//            expectPeak (*this, *edit, { 1_tp, 2_tp }, {}, 1.0f);
//            expectPeak (*this, *edit, { 2_tp, 3_tp }, {}, 2.0f);
//            expectPeak (*this, *edit, { 3_tp, 4_tp }, {}, 1.0f);
//            expectPeak (*this, *edit, { 4_tp, 5_tp }, {}, 0.0f);
        }
    }
};

static ContainerClipTests containerClipTests;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_UNIT_TESTS
