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
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        runNodePreparationBenchmarks (engine);
        runLargeGraphUpdateBenchmark (engine);
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

    void runLargeGraphUpdateBenchmark (Engine& engine)
    {
        constexpr int trackCount = 200;
        auto edit = Edit::createSingleTrackEdit (engine);

        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        {
            auto audioTrack = edit->insertNewAudioTrack (TrackInsertPoint { nullptr, nullptr }, nullptr);
            const auto pluginTypesToAdd =
            {
                AuxReturnPlugin::xmlTypeName,
                PatchBayPlugin::xmlTypeName,
                AuxReturnPlugin::xmlTypeName,
                PatchBayPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                AuxSendPlugin::xmlTypeName,
                PatchBayPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                VolumeAndPanPlugin::xmlTypeName,
                AuxSendPlugin::xmlTypeName,
                PatchBayPlugin::xmlTypeName
            };

            for (auto pluginType : pluginTypesToAdd)
            {
                auto plugin = edit->getPluginCache().createNewPlugin (pluginType, {});
                jassert (plugin.get() != nullptr);

                const int commonBusNumber = 1000;

                if (auto returnPlugin = dynamic_cast<AuxReturnPlugin*> (plugin.get()))
                {
                    returnPlugin->busNumber.setValue (trackIndex, nullptr);
                }
                else if (auto sendPlugin = dynamic_cast<AuxSendPlugin*> (plugin.get()))
                {
                    // Sending all sends to the same return is approximately 3x more time
                    // compared to sending each to a different return:
                    sendPlugin->busNumber.setValue (commonBusNumber, nullptr);
                }

                audioTrack->pluginList.insertPlugin (plugin, -1, nullptr);
            }
        }

        {
            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, edit->tempoSequence };
            CreateNodeParams cnp { processState, 44100.0, 256 };

            {
                auto editNode = createNodeForEdit (*edit, cnp);

                const ScopedBenchmark sb (createBenchmarkDescription (
                    "Node Preparation",
                    juce::String ("Preparing 123 tracks for busses").replace ("123", juce::String (trackCount)).toStdString(),
                    juce::String ("Preparing with 14 AuxSend, AuxReturn and PatchBay Plugins").toStdString()));

                [[ maybe_unused ]] auto graph = node_player_utils::prepareToPlay (std::move (editNode), nullptr,
                                                                                  cnp.sampleRate, cnp.blockSize);
            }
        }
    }
};

static PluginNodeBenchmarks pluginNodeBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif
