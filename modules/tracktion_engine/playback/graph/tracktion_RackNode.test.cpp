/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if GRAPH_UNIT_TESTS_RACKNODE

using namespace tracktion::graph;

//==============================================================================
//==============================================================================
class RackNodeTests : public juce::UnitTest
{
public:
    RackNodeTests()
        : juce::UnitTest ("RackNode", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<ToneGeneratorPlugin>();
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();
                
        runAllTests<tracktion::graph::NodePlayer>();
        runAllTests<tracktion::graph::LockFreeMultiThreadedNodePlayer>();
    }
    
    template<typename NodePlayerType>
    void runAllTests()
    {
       auto start = std::chrono::high_resolution_clock::now();
                
        for (auto setup : test_utilities::getTestSetups (*this))
        {
            logMessage (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N"));

            // Rack tests
            runRackTests<NodePlayerType> (setup);
            runRackAudioInputTests<NodePlayerType> (setup);
            runRackModifiertests<NodePlayerType> (setup);
        }

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        logMessage (juce::String ("Tests for ") + typeid (NodePlayerType).name() + " - "
                    + juce::String (std::chrono::duration_cast<std::chrono::milliseconds> (elapsed).count()) + "ms");
    }

private:
    //==============================================================================
    template<typename NodePlayerType>
    void runRackTests (test_utilities::TestSetup testSetup)
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        
        beginTest ("Unconnected Rack");
        {
            // Rack with a sin oscilator but not connected should be silent
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);

            auto rack = edit->getRackList().addNewRack();
            expect (rack != nullptr);
            expectEquals (rack->getConnections().size(), 0);
            expectEquals (rack->getInputNames().size(), 3);
            expectEquals (rack->getOutputNames().size(), 3);

            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            expect (tonePlugin != nullptr);

            rack->addPlugin (tonePlugin, {}, false);
            expect (rack->getPlugins().getFirst() == pluginPtr.get());

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Basic sin Rack connected to inputs");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            expect (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            expect (rack != nullptr);
            expect (rack->getPlugins().getFirst() == pluginPtr.get());
            expectEquals (rack->getConnections().size(), 6);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Basic sin only connected to outputs");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto rack = edit->getRackList().addNewRack();
            expectEquals (rack->getOutputNames().size(), 3);

            auto tonePlugin = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            rack->addPlugin (tonePlugin, {}, false);

            rack->addConnection (tonePlugin->itemID, 1, {}, 1);
            expectEquals (rack->getConnections().size(), 1);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 1, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Four channel sin Rack");
        {
            // This Rack has four input and output channels
            // The single sin node goes to all the outputs
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            expect (tonePlugin != nullptr);

            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            rack->addOutput (3, "Bus L");
            rack->addOutput (4, "Bus R");

            rack->addConnection (tonePlugin->itemID, 1, {}, 3);
            rack->addConnection (tonePlugin->itemID, 2, {}, 4);

            expectEquals (rack->getConnections().size(), 8);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);

                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                for (int c : { 0, 1, 2, 3 })
                    test_utilities::expectAudioBuffer (*this, testContext->buffer, c, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
        
        beginTest ("Two sins in parallel Rack");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            tonePlugin->levelParam->setParameter (0.5f, juce::dontSendNotification);
            expect (tonePlugin != nullptr);
            
            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            expect (rack != nullptr);
            
            // Add another ToneGenerator and connect it in parallel
            Plugin::Ptr secondToneGen = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            dynamic_cast<ToneGeneratorPlugin*> (secondToneGen.get())->levelParam->setParameter (0.5f, juce::dontSendNotification);
            rack->addPlugin (secondToneGen, {}, false);
            rack->addConnection ({}, 0, secondToneGen->itemID, 0);
            rack->addConnection ({}, 1, secondToneGen->itemID, 1);
            rack->addConnection ({}, 2, secondToneGen->itemID, 2);
            rack->addConnection (secondToneGen->itemID, 0, {}, 0);
            rack->addConnection (secondToneGen->itemID, 1, {}, 1);
            rack->addConnection (secondToneGen->itemID, 2, {}, 2);

            expectEquals (rack->getPlugins().size(), 2);
            expectEquals (rack->getConnections().size(), 12);
            
            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                                        
                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
            }
                    
            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Two sins in parallel, one delayed Rack");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            tonePlugin->levelParam->setParameter (0.5f, juce::dontSendNotification);
            expect (tonePlugin != nullptr);
            
            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            expect (rack != nullptr);
            
            // Add another ToneGenerator feeding in to a LatencyPlugin and connect it in parallel
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

            expectEquals (rack->getPlugins().size(), 3);
            expectEquals (rack->getConnections().size(), 15);
            
            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                                        
                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                const int latencyNumSamples = juce::roundToInt (latencyTimeInSeconds * testSetup.sampleRate);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 1, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
        
        beginTest ("Two paths to single synth");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto rack = edit->getRackList().addNewRack();
            expectEquals (rack->getOutputNames().size(), 3);

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
            expectEquals (rack->getConnections().size(), 4);

            dynamic_cast<ToneGeneratorPlugin*> (tonePlugin.get())->levelParam->setParameter (0.5f, juce::dontSendNotification);
            
            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                                        
                auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
            }
                    
            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    template<typename NodePlayerType>
    void runRackAudioInputTests (test_utilities::TestSetup testSetup)
    {
        auto& engine = *Engine::getEngines()[0];
        
        // These tests won't work with random block sizes as the test inputs are just static
        if (! testSetup.randomiseBlockSizes)
        {
            beginTest ("Basic sin audio input Rack");
            {
                // Just a stereo sin input connected directly to the output across 4 channels
                auto edit = Edit::createSingleTrackEdit (engine);

                Plugin::Array plugins;
                auto rack = edit->getRackList().addNewRack();
                expect (rack != nullptr);
                
                rack->addInput (3, "Bus In L");
                rack->addInput (4, "Bus In R");
                rack->addOutput (3, "Bus Out L");
                rack->addOutput (4, "Bus Out R");

                for (int p : { 0, 1, 2, 3, 4 })
                    rack->addConnection ({}, p, {}, p);
                
                expectEquals (rack->getConnections().size(), 5);

                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (4, (choc::buffer::FrameCount) testSetup.blockSize);

                // Fill inputs with sin data
                {
                    test_utilities::fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                // Process Rack
                {
                    graph::PlayHead ph;
                    PlayHeadState phs (ph);
                    ProcessState ps (phs);
                    auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                    test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);
                    auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                    auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                    for (int c : { 0, 1, 2, 3 })
                        test_utilities::expectAudioBuffer (*this, testContext->buffer, c, 1.0f, 0.707f);
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

                    expectEquals (rack->getConnections().size(), 7);

                    // Process Rack
                    {
                        graph::PlayHead ph;
                        PlayHeadState phs (ph);
                        ProcessState ps (phs);
                        auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                        test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);
                        auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                        auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                        for (int c : { 0, 1, 2, 3 })
                            test_utilities::expectAudioBuffer (*this, testContext->buffer, c, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
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
                        test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);
                        auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                        auto testContext = createTestContext (std::move (rackProcessor), testSetup, 4, 5.0);

                        // Channel 0 should be a sin from 0.5s, silent before
                        test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples,
                                                           0.0f, 0.0f, 1.0f, 0.707f);

                        // The others should be silent
                        for (int c : { 1, 2, 3 })
                            test_utilities::expectAudioBuffer (*this, testContext->buffer, c, 0.0f, 0.0f);
                    }
                }
                        
                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
            }
            
            beginTest ("Mismatched num input and Rack channels");
            {
                // Just a stereo sin input connected directly to the output across 2 channels
                auto edit = Edit::createSingleTrackEdit (engine);

                Plugin::Array plugins;
                auto rack = edit->getRackList().addNewRack();
                expect (rack != nullptr);

                for (int p : { 0, 1, 2 })
                    rack->addConnection ({}, p, {}, p);

                expectEquals (rack->getConnections().size(), 3);

                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (1, (choc::buffer::FrameCount) testSetup.blockSize);

                // Fill inputs with sin data
                {
                    test_utilities::fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                // Process Rack
                {
                    graph::PlayHead ph;
                    PlayHeadState phs (ph);
                    ProcessState ps (phs);
                    auto rackNode = RackNodeBuilder::createRackNode (*rack, testSetup.sampleRate, testSetup.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                    test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);
                    auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), testSetup.sampleRate, testSetup.blockSize);
                    auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);

                    // Channel 0 should be a sin, channel 1 silent
                    test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
                    test_utilities::expectAudioBuffer (*this, testContext->buffer, 1, 0.0f, 0.0f);
                }

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
            }
        }
    }
    
    template<typename NodePlayerType>
    void runRackModifiertests (test_utilities::TestSetup ts)
    {
        auto& engine = *Engine::getEngines()[0];
        
        beginTest ("LFO Modifier Rack");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto tonePlugin = dynamic_cast<ToneGeneratorPlugin*> (pluginPtr.get());
            expect (tonePlugin != nullptr);
            
            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            expect (rack != nullptr);
            expect (rack->getPlugins().getFirst() == pluginPtr.get());
            expectEquals (rack->getConnections().size(), 6);
            
            auto modifier = rack->getModifierList().insertModifier (juce::ValueTree (IDs::LFO), 0, nullptr);
            auto lfoModifier = dynamic_cast<LFOModifier*> (modifier.get());
            lfoModifier->depthParam->setParameter (0.0f, juce::dontSendNotification);
            lfoModifier->offsetParam->setParameter (0.5f, juce::dontSendNotification);
            expectWithinAbsoluteError (lfoModifier->depthParam->getCurrentValue(), 0.0f, 0.001f);
            expectWithinAbsoluteError (lfoModifier->offsetParam->getCurrentValue(), 0.5f, 0.001f);
            
            tonePlugin->levelParam->addModifier (*modifier, -1.0f);
            
            edit->updateModifierTimers ({}, 0);
            tonePlugin->levelParam->updateToFollowCurve ({}); // Force an update of the param value for testing
            expectWithinAbsoluteError (lfoModifier->getCurrentValue(), 0.5f, 0.001f);
            expectWithinAbsoluteError (tonePlugin->levelParam->getCurrentValue(), 0.5f, 0.001f);

            // Process Rack
            {
                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, ts.sampleRate, ts.blockSize, makeNode<SilentNode> (2), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), ts.sampleRate, ts.blockSize);
                                        
                auto testContext = createTestContext (std::move (rackProcessor), ts, 2, 5.0);
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.5f, 0.353f);
            }

            // Check this hasn't changed
            expectWithinAbsoluteError (tonePlugin->levelParam->getCurrentValue(), 0.5f, 0.001f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Envelope Modifier Rack");
        {
            // This has a stereo input to an Envelope follower configured to output an envelope of 1 with a sin input
            // This then controls the level of a volume plugin to -6dB
            
            auto edit = Edit::createSingleTrackEdit (engine);
            auto track = getFirstAudioTrack (*edit);
            
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin (pluginPtr, 0, nullptr);
            auto volPlugin = dynamic_cast<VolumeAndPanPlugin*> (pluginPtr.get());
            expect (volPlugin != nullptr);
            
            Plugin::Array plugins;
            plugins.add (pluginPtr);
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            expect (rack != nullptr);
            expect (rack->getPlugins().getFirst() == pluginPtr.get());
            expectEquals (rack->getConnections().size(), 6);

            auto modifier = rack->getModifierList().insertModifier (juce::ValueTree (IDs::ENVELOPEFOLLOWER), 0, nullptr);
            auto envelopeModifier = dynamic_cast<EnvelopeFollowerModifier*> (modifier.get());
            envelopeModifier->attackParam->setParameter (envelopeModifier->attackParam->valueRange.start, juce::dontSendNotification);
            envelopeModifier->releaseParam->setParameter (envelopeModifier->releaseParam->valueRange.end, juce::dontSendNotification);
            expectWithinAbsoluteError (envelopeModifier->attackParam->getCurrentValue(), 1.0f, 0.001f);
            expectWithinAbsoluteError (envelopeModifier->releaseParam->getCurrentValue(), 5000.0f, 0.001f);
            
            rack->addConnection ({}, 1, envelopeModifier->itemID, 0);
            rack->addConnection ({}, 2, envelopeModifier->itemID, 1);
            expectEquals (rack->getConnections().size(), 8);

            // This value should modify the volume to -6dB
            volPlugin->volParam->addModifier (*modifier, -0.193f);
            
            edit->updateModifierTimers ({}, 0);
            volPlugin->updateActiveParameters();
            volPlugin->volParam->updateToFollowCurve ({}); // Force an update of the param value for testing

            // Process Rack
            {
                // Sin input provider
                const auto inputProvider = std::make_shared<InputProvider>();
                choc::buffer::ChannelArrayBuffer<float> inputBuffer (2, (choc::buffer::FrameCount) ts.blockSize);

                // Fill inputs with sin data
                {
                    test_utilities::fillBufferWithSinData (inputBuffer);
                    tracktion::engine::MidiMessageArray midi;
                    inputProvider->setInputs ({ inputBuffer, midi });
                }

                graph::PlayHead ph;
                PlayHeadState phs (ph);
                ProcessState ps (phs);
                auto rackNode = RackNodeBuilder::createRackNode (*rack, ts.sampleRate, ts.blockSize, makeNode<InputNode> (inputProvider), ps, true);
                test_utilities::expectUniqueNodeIDs (*this, *rackNode, true);

                auto rackProcessor = std::make_unique<RackNodePlayer<NodePlayerType>> (std::move (rackNode), ts.sampleRate, ts.blockSize);
                                        
                auto testContext = createTestContext (std::move (rackProcessor), ts, 2, 5.0);

                // Disable this test for now until full automation is working
               #if 0
                ignoreUnused (testContext);
                // Trim the first 0.1s as the envelope ramps up
                const juce::Range<int> sampleRange (roundToInt (0.5 * ts.sampleRate), roundToInt (5.0 * ts.sampleRate));
                test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, sampleRange, 0.5f, 0.353f);
               #endif
            }

            // Check this ends on -6db
            expectWithinAbsoluteError (volPlugin->getVolumeDb(), -6.0f, 0.1f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }
};

static RackNodeTests rackNodeTests;

#endif //TRACKTION_UNIT_TESTS

}} // namespace tracktion { inline namespace engine
