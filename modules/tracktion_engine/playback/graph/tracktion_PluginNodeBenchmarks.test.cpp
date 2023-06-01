/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_PLUGINNODE

#include "tracktion_BenchmarkUtilities.h"


namespace tracktion { inline namespace engine
{

using namespace tracktion::graph;

//==============================================================================
//==============================================================================
class PluginNodeBenchmarks : public juce::UnitTest
{
public:
    PluginNodeBenchmarks()
        : juce::UnitTest ("Plugin Node Benchmarks", "tracktion_benchmarks")
    {
    }
    
    void runTest() override
    {
//        using namespace benchmark_utilities;
//        using namespace tracktion::graph;
//        test_utilities::TestSetup ts;
//        ts.sampleRate = 96000.0;
//        ts.blockSize = 128;

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        runNodePreparationBenchmarks (engine);
    }

private:
    void runNodePreparationBenchmarks (Engine& engine)
    {
        const juce::StringArray pluginTypesToTest { VolumeAndPanPlugin::xmlTypeName,
                                                    AuxSendPlugin::xmlTypeName,
                                                    AuxReturnPlugin::xmlTypeName,
                                                    PatchBayPlugin::xmlTypeName };

        for (auto pluginType : pluginTypesToTest)
            runNodePreparationBenchmark (engine, pluginType);
    }

    void runNodePreparationBenchmark (Engine& engine, juce::String pluginType)
    {
        constexpr int trackCount = 200;
        auto edit = Edit::createSingleTrackEdit (engine);

        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        {
            auto audioTrack = edit->insertNewAudioTrack (TrackInsertPoint {nullptr, nullptr}, nullptr);
            auto plugin = edit->getPluginCache().createNewPlugin (pluginType, {});
            jassert (plugin.get() != nullptr);
            audioTrack->pluginList.insertPlugin (plugin, -1, nullptr);
        }

        {
            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, edit->tempoSequence };
            CreateNodeParams cnp { processState, 44100.0, 256 };

            auto editNode = createNodeForEdit (*edit, cnp);

            const ScopedBenchmark sb (createBenchmarkDescription ("Node Preparation",
                                                                  juce::String ("Preparing with XXYY Plugin")
                                                                    .replace ("XXYY", pluginType).toStdString(),
                                                                  juce::String ("Preparing 123 tracks with XXYY plugin")
                                                                    .replace ("123", juce::String (trackCount))
                                                                    .replace ("XXYY", juce::String (pluginType)).toStdString()));

            [[ maybe_unused ]] auto graph = node_player_utils::prepareToPlay (std::move (editNode), nullptr,
                                                                              cnp.sampleRate, cnp.blockSize);
        }
    }
};

static PluginNodeBenchmarks pluginNodeBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif
