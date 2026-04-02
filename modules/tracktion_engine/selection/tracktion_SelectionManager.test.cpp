/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_SELECTABLE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

static BenchmarkDescription getSelectableBenchmarkDescription (std::string bmName)
{
    const auto bmCategory = std::string ("Selectable/tracktion_benchmarks");
    const auto bmDescription = bmName;

    return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
             bmCategory, bmName, bmDescription };
}

TEST_SUITE ("tracktion_benchmarks")
{
    TEST_CASE ("Selectable Benchmarks")
    {
        // Create an empty edit
        // Create an Edit with 100 clips on each of 100 tracks (10'000 total)

        constexpr int numClipsPerTrack = 100;
        constexpr int numTracks = 100;

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);
        SelectionManager sm (engine);

        edit->ensureNumberOfAudioTracks (numTracks);
        auto audioTracks = getAudioTracks (*edit);
        SelectableList clips;

        for (int t = 0; t < numTracks; ++t)
        {
            auto at = audioTracks[t];

            for (int i = 0; i < numClipsPerTrack; ++ i)
            {
                auto c = at->insertMIDIClip ({ 1_tp * i, 1_tp * (i + 1) }, nullptr);
                clips.add (c.get());
            }
        }

        {
            ScopedBenchmark sb (getSelectableBenchmarkDescription ("Select 10,000 clips"));
            sm.select (clips);
        }

        {
            ScopedBenchmark sb (getSelectableBenchmarkDescription ("getClipSelectionWithCollectionClipContents"));
            [[ maybe_unused ]]auto l = getClipSelectionWithCollectionClipContents (clips);
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS
