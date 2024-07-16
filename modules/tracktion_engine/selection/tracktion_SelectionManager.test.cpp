/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_SELECTABLE

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
class SelectableBenchmarks  : public juce::UnitTest
{
public:
    SelectableBenchmarks()
        : juce::UnitTest ("Selectable", "tracktion_benchmarks")
    {}

    void runTest() override
    {
        // Create an empty edit
        // Create an Edit with 100 clips on each of 100 tracks (10'000 total)

        constexpr int numClipsPerTrack = 100;
        constexpr int numTracks = 100;

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);
        SelectionManager sm (engine);

        beginTest ("Selectable Benchmarks");
        {
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
                ScopedBenchmark sb (getDescription ("Select 10,000 clips"));
                sm.select (clips);
            }

            {
                ScopedBenchmark sb (getDescription ("getClipSelectionWithCollectionClipContents"));
                [[ maybe_unused ]]auto l = getClipSelectionWithCollectionClipContents (clips);
            }
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

static SelectableBenchmarks selectableBenchmarks;

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS
