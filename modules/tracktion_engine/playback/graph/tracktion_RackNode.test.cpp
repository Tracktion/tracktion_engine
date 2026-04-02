/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_RACKNODE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

using namespace tracktion::graph;
using namespace tracktion::graph::test_utilities;

//==============================================================================
namespace racknode_test_helpers
{
    template<typename NodePlayerType>
    static void runRackTests (TestSetup testSetup)
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        // Unconnected Rack
        {
            // Rack with a sin oscilator but not connected should be silent
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);

            auto rack = edit->getRackList().addNewRack();
            CHECK (rack != nullptr);
            CHECK_EQ (rack->getConnections().size(), 0);
            CHECK_EQ (rack->getInputNames().size(), 3);
            CHECK_EQ (rack->getOutputNames().size(), 3);

            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            CHECK (tonePlugin != nullptr);

            rack->addPlugin (tonePlugin, {}, false);
            CHECK (rack->getPlugins().getFirst() == pluginPtr.get());

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 0.0f, 0.0f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Basic sin Rack connected to inputs
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            CHECK (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            CHECK (rack != nullptr);
            CHECK (rack->getPlugins().getFirst() == pluginPtr.get());
            CHECK_EQ (rack->getConnections().size(), 6);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Basic sin only connected to outputs
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto rack = edit->getRackList().addNewRack();
            CHECK_EQ (rack->getOutputNames().size(), 3);

            auto tonePlugin = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            rack->addPlugin (tonePlugin, {}, false);

            rack->addConnection (tonePlugin->itemID, 1, {}, 1);
            CHECK_EQ (rack->getConnections().size(), 1);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 1, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Four channel sin Rack
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            CHECK (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            rack->addOutput (3, "Bus L");
            rack->addOutput (4, "Bus R");

            rack->addConnection (tonePlugin->itemID, 1, {}, 3);
            rack->addConnection (tonePlugin->itemID, 2, {}, 4);

            CHECK_EQ (rack->getConnections().size(), 8);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                for (int c : { 0, 1, 2, 3 })
                    expectAudioBuffer (testContext->buffer, c, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Two sins in parallel Rack
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            tonePlugin->levelParam->setParameter (0.5f, juce::dontSendNotification);
            CHECK (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            CHECK (rack != nullptr);

            Plugin::Ptr secondToneGen = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            dynamic_cast<ToneGeneratorPlugin*> (secondToneGen.get())->levelParam->setParameter (0.5f, juce::dontSendNotification);
            rack->addPlugin (secondToneGen, {}, false);
            rack->addConnection ({}, 0, secondToneGen->itemID, 0);
            rack->addConnection ({}, 1, secondToneGen->itemID, 1);
            rack->addConnection ({}, 2, secondToneGen->itemID, 2);
            rack->addConnection (secondToneGen->itemID, 0, {}, 0);
            rack->addConnection (secondToneGen->itemID, 1, {}, 1);
            rack->addConnection (secondToneGen->itemID, 2, {}, 2);

            CHECK_EQ (rack->getPlugins().size(), 2);
            CHECK_EQ (rack->getConnections().size(), 12);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Two sins in parallel, one delayed Rack
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            tonePlugin->levelParam->setParameter (0.5f, juce::dontSendNotification);
            CHECK (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            CHECK (rack != nullptr);

            Plugin::Ptr secondToneGen = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            dynamic_cast<ToneGeneratorPlugin*> (secondToneGen.get())->levelParam->setParameter (0.5f, juce::dontSendNotification);

            Plugin::Ptr latencyPlugin = edit->getPluginCache().createNewPlugin (LatencyPlugin::xmlTypeName, {});
            const double latencyTimeInSeconds = 0.5f;
            dynamic_cast<LatencyPlugin*> (latencyPlugin.get())->latencyTimeSeconds = latencyTimeInSeconds;

            rack->addPlugin (secondToneGen, {}, false);
            rack->addPlugin (latencyPlugin, {}, false);

            rack->addConnection ({}, 0, secondToneGen->itemID, 0);
            rack->addConnection ({}, 1, secondToneGen->itemID, 1);
            rack->addConnection ({}, 2, secondToneGen->itemID, 2);
            rack->addConnection (secondToneGen->itemID, 0, latencyPlugin->itemID, 0);
            rack->addConnection (secondToneGen->itemID, 1, latencyPlugin->itemID, 1);
            rack->addConnection (secondToneGen->itemID, 2, latencyPlugin->itemID, 2);
            rack->addConnection (latencyPlugin->itemID, 0, {}, 0);
            rack->addConnection (latencyPlugin->itemID, 1, {}, 1);
            rack->addConnection (latencyPlugin->itemID, 2, {}, 2);

            CHECK_EQ (rack->getPlugins().size(), 3);
            CHECK_EQ (rack->getConnections().size(), 15);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                const int latencyNumSamples = juce::roundToInt (latencyTimeInSeconds * testSetup.sampleRate);
                expectAudioBuffer (testContext->buffer, 0, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
                expectAudioBuffer (testContext->buffer, 1, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Two paths to single synth
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto rack = edit->getRackList().addNewRack();
            CHECK_EQ (rack->getOutputNames().size(), 3);

            auto tonePlugin = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            rack->addPlugin (tonePlugin, {}, false);
            auto vol1Plugin = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
            rack->addPlugin (vol1Plugin, {}, false);
            auto vol2Plugin = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
            rack->addPlugin (vol2Plugin, {}, false);

            rack->addConnection (tonePlugin->itemID, 1, vol1Plugin->itemID, 1);
            rack->addConnection (tonePlugin->itemID, 1, vol2Plugin->itemID, 1);
            rack->addConnection (vol1Plugin->itemID, 1, {}, 1);
            rack->addConnection (vol2Plugin->itemID, 1, {}, 1);
            CHECK_EQ (rack->getConnections().size(), 4);

            dynamic_cast<ToneGeneratorPlugin*> (tonePlugin.get())->levelParam->setParameter (0.5f, juce::dontSendNotification);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    template<typename NodePlayerType>
    static void runRackAudioInputTests (TestSetup testSetup)
    {
        auto& engine = *Engine::getEngines()[0];

        // These tests won't work with random block sizes as the test inputs are just static
        if (! testSetup.randomiseBlockSizes)
        {
            // Basic sin audio input Rack
            {
                auto edit = Edit::createSingleTrackEdit (engine);

                Plugin::Array plugins;
                auto rack = edit->getRackList().addNewRack();
                CHECK (rack != nullptr);

                rack->addInput (3, "Bus In L");
                rack->addInput (4, "Bus In R");
                rack->addOutput (3, "Bus Out L");
                rack->addOutput (4, "Bus Out R");

                for (int p : { 0, 1, 2, 3, 4 })
                    rack->addConnection ({}, p, {}, p);

                CHECK_EQ (rack->getConnections().size(), 5);

                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (4, (choc::buffer::FrameCount) testSetup.blockSize);

                // Fill inputs with sin data
                {
                    fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                // Process Rack
                {
                    graph::PlayHead ph;
                    PlayHeadState phs (ph);
                    ProcessState ps (phs);
                    auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                    expectUniqueNodeIDs (*rackNode, true);
                    auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                    auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                    for (int c : { 0, 1, 2, 3 })
                        expectAudioBuffer (testContext->buffer, c, 1.0f, 0.707f);
                }

                // Remove connections between 3 & 4, add a latency plugin there, the results should be the same
                {
                    rack->removeConnection ({}, 3, {}, 3);
                    rack->removeConnection ({}, 4, {}, 4);

                    Plugin::Ptr latencyPlugin = edit->getPluginCache().createNewPlugin (LatencyPlugin::xmlTypeName, {});
                    const double latencyTimeInSeconds = 0.5f;
                    const int latencyNumSamples = juce::roundToInt (latencyTimeInSeconds * testSetup.sampleRate);
                    dynamic_cast<LatencyPlugin*> (latencyPlugin.get())->latencyTimeSeconds = latencyTimeInSeconds;

                    rack->addPlugin (latencyPlugin, {}, false);

                    rack->addConnection ({}, 3, latencyPlugin->itemID, 1);
                    rack->addConnection ({}, 4, latencyPlugin->itemID, 2);
                    rack->addConnection (latencyPlugin->itemID, 1, {}, 3);
                    rack->addConnection (latencyPlugin->itemID, 2, {}, 4);

                    CHECK_EQ (rack->getConnections().size(), 7);

                    // Process Rack
                    {
                        graph::PlayHead ph;
                        PlayHeadState phs (ph);
                        ProcessState ps (phs);
                        auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                        expectUniqueNodeIDs (*rackNode, true);
                        auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                        auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                        for (int c : { 0, 1, 2, 3 })
                            expectAudioBuffer (testContext->buffer, c, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
                    }

                    // Set the num audio inputs to be 1 channel and the Rack shouldn't crash
                    {
                        inputProvider->numChannels = 1;
                        tracktion::engine::MidiMessageArray midi;
                        inputProvider->setInputs ({ inputBuffer, midi });

                        graph::PlayHead ph;
                        PlayHeadState phs (ph);
                        ProcessState ps (phs);
                        auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                        expectUniqueNodeIDs (*rackNode, true);
                        auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                        auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                        // Channel 0 should be a sin from 0.5s, silent before
                        expectAudioBuffer (testContext->buffer, 0, latencyNumSamples,
                                                           0.0f, 0.0f, 1.0f, 0.707f);

                        // The others should be silent
                        for (int c : { 1, 2, 3 })
                            expectAudioBuffer (testContext->buffer, c, 0.0f, 0.0f);
                    }
                }

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
            }

            // Mismatched num input and Rack channels
            {
                auto edit = Edit::createSingleTrackEdit (engine);

                Plugin::Array plugins;
                auto rack = edit->getRackList().addNewRack();
                CHECK (rack != nullptr);

                for (int p : { 0, 1, 2 })
                    rack->addConnection ({}, p, {}, p);

                CHECK_EQ (rack->getConnections().size(), 3);

                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (1, (choc::buffer::FrameCount) testSetup.blockSize);

                // Fill inputs with sin data
                {
                    fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                // Process Rack
                {
                    graph::PlayHead ph;
                    PlayHeadState phs (ph);
                    ProcessState ps (phs);
                    auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                    expectUniqueNodeIDs (*rackNode, true);
                    auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                    auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);

                    // Channel 0 should be a sin, channel 1 silent
                    expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
                    expectAudioBuffer (testContext->buffer, 1, 0.0f, 0.0f);
                }

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
            }
        }
    }

    template<typename NodePlayerType>
    static void runRackModifierTests (TestSetup ts)
    {
        auto& engine = *Engine::getEngines()[0];

        // LFO Modifier Rack
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            CHECK (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            CHECK (rack != nullptr);
            CHECK (rack->getPlugins().getFirst() == pluginPtr.get());
            CHECK_EQ (rack->getConnections().size(), 6);

            auto modifier = rack->getModifierList().insertModifier (juce::ValueTree (IDs::LFO), 0, nullptr);
            auto lfoModifier = dynamic_cast<LFOModifier*> (modifier.get());
            lfoModifier->depthParam->setParameter (0.0f, juce::dontSendNotification);
            lfoModifier->offsetParam->setParameter (0.5f, juce::dontSendNotification);
            CHECK (std::abs (lfoModifier->depthParam->getCurrentValue() - 0.0f) <= 0.001f);
            CHECK (std::abs (lfoModifier->offsetParam->getCurrentValue() - 0.5f) <= 0.001f);

            tonePlugin->levelParam->addModifier (*modifier, -1.0f);

            edit->updateModifierTimers ({}, 0);
            tonePlugin->levelParam->updateToFollowCurve ({}); // Force an update of the param value for testing
            CHECK (std::abs (lfoModifier->getCurrentValue() - 0.5f) <= 0.001f);
            CHECK (std::abs (tonePlugin->levelParam->getCurrentValue() - 0.5f) <= 0.001f);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, ts.sampleRate, ts.blockSize, makeNode<SilentNode> (2), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), ts.sampleRate, ts.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), ts, 2, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 0.5f, 0.353f);
            }

            // Check this hasn't changed
            CHECK (std::abs (tonePlugin->levelParam->getCurrentValue() - 0.5f) <= 0.001f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        // Envelope Modifier Rack
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);

            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto volPlugin = dynamic_cast<VolumeAndPanPlugin*> (pluginPtr.get());
            CHECK (volPlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            CHECK (rack != nullptr);
            CHECK (rack->getPlugins().getFirst() == pluginPtr.get());
            CHECK_EQ (rack->getConnections().size(), 6);

            auto modifier = rack->getModifierList().insertModifier (juce::ValueTree (IDs::ENVELOPEFOLLOWER), 0, nullptr);
            auto envelopeModifier = dynamic_cast<EnvelopeFollowerModifier*> (modifier.get());
            envelopeModifier->attackParam->setParameter (envelopeModifier->attackParam->valueRange.start, juce::dontSendNotification);
            envelopeModifier->releaseParam->setParameter (envelopeModifier->releaseParam->valueRange.end, juce::dontSendNotification);
            CHECK (std::abs (envelopeModifier->attackParam->getCurrentValue() - 1.0f) <= 0.001f);
            CHECK (std::abs (envelopeModifier->releaseParam->getCurrentValue() - 5000.0f) <= 0.001f);

            rack->addConnection ({}, 1, envelopeModifier->itemID, 0);
            rack->addConnection ({}, 2, envelopeModifier->itemID, 1);
            CHECK_EQ (rack->getConnections().size(), 8);

            // This value should modify the volume to -6dB
            volPlugin->volParam->addModifier (*modifier, -0.193f);

            edit->updateModifierTimers ({}, 0);
            volPlugin->volParam->updateToFollowCurve ({}); // Force an update of the param value for testing

            // Process Rack
            {
                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (2, (choc::buffer::FrameCount) ts.blockSize);

                // Fill inputs with sin data
                {
                    fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, ts.sampleRate, ts.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                expectUniqueNodeIDs (*rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), ts.sampleRate, ts.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), ts, 2, 5.0);

                // Disable this test for now until full automation is working
               #if 0
                ignoreUnused (testContext);
                // Trim the first 0.1s as the envelope ramps up
                const juce::Range<int> sampleRange (roundToInt (0.5 * ts.sampleRate), roundToInt (5.0 * ts.sampleRate));
                expectAudioBuffer (testContext->buffer, 0, sampleRange, 0.5f, 0.353f);
               #endif
            }

            // Check this ends on -6db
            CHECK (std::abs (volPlugin->getVolumeDb() - (-6.0f)) <= 0.1f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    template<typename NodePlayerType>
    static void runAllTests()
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (auto setup : getTestSetups())
        {
            MESSAGE (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N").toRawUTF8());

            // Rack tests
            runRackTests<NodePlayerType> (setup);
            runRackAudioInputTests<NodePlayerType> (setup);
            runRackModifierTests<NodePlayerType> (setup);
        }

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        MESSAGE (juce::String ("Tests for ") * juce::String (typeid (NodePlayerType).name()) * " - "
                 * juce::String (std::chrono::duration_cast<std::chrono::milliseconds> (elapsed).count()) * "ms");
    }
} // namespace racknode_test_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("RackNode")
{
    auto& engine = *tracktion::engine::Engine::getEngines()[0];
    engine.getPluginManager().createBuiltInType<ToneGeneratorPlugin>();
    engine.getPluginManager().createBuiltInType<LatencyPlugin>();

    racknode_test_helpers::runAllTests<tracktion::graph::NodePlayer>();
    racknode_test_helpers::runAllTests<tracktion::graph::LockFreeMultiThreadedNodePlayer>();
}

} // TEST_SUITE

#endif //TRACKTION_UNIT_TESTS

} // namespace tracktion::inline engine
