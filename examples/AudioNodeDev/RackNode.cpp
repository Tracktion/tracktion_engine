/*
  ==============================================================================

    RackNode.cpp
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#include "RackNode.h"

void AudioNodeTests::runRackTests (TestSetup testSetup)
{
    using namespace tracktion_engine;
    auto& engine = *Engine::getEngines()[0];
    engine.getPluginManager().createBuiltInType<ToneGeneratorPlugin>();
    engine.getPluginManager().createBuiltInType<LatencyPlugin>();

    beginTest ("Basic sin Rack");
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
            auto inputProvider = std::make_shared<InputProvider>();
            auto rackNode = RackNodeBuilder::createRackAudioNode (*rack, inputProvider);

            auto rackProcessor = std::make_unique<RackAudioNodeProcessor> (std::move (rackNode), inputProvider);
                                    
            auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
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
        tonePlugin->levelParam->setParameter (0.5f, dontSendNotification);
        expect (tonePlugin != nullptr);
        
        Plugin::Array plugins;
        plugins.add (pluginPtr);
        auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
        expect (rack != nullptr);
        
        // Add another ToneGenerator and connect it in parallel
        Plugin::Ptr secondToneGen = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
        dynamic_cast<ToneGeneratorPlugin*> (secondToneGen.get())->levelParam->setParameter (0.5f, dontSendNotification);
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
            auto inputProvider = std::make_shared<InputProvider>();
            auto rackNode = RackNodeBuilder::createRackAudioNode (*rack, inputProvider);

            auto rackProcessor = std::make_unique<RackAudioNodeProcessor> (std::move (rackNode), inputProvider);
                                    
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
        tonePlugin->levelParam->setParameter (0.5f, dontSendNotification);
        expect (tonePlugin != nullptr);
        
        Plugin::Array plugins;
        plugins.add (pluginPtr);
        auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
        expect (rack != nullptr);
        
        // Add another ToneGenerator feeding in to a LatencyPlugin and connect it in parallel
        Plugin::Ptr secondToneGen = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
        dynamic_cast<ToneGeneratorPlugin*> (secondToneGen.get())->levelParam->setParameter (0.5f, dontSendNotification);

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
            auto inputProvider = std::make_shared<InputProvider>();
            auto rackNode = RackNodeBuilder::createRackAudioNode (*rack, inputProvider);

            auto rackProcessor = std::make_unique<RackAudioNodeProcessor> (std::move (rackNode), inputProvider);
                                    
            auto testContext = createTestContext (std::move (rackProcessor), testSetup, 2, 5.0);
            const int latencyNumSamples = roundToInt (latencyTimeInSeconds * testSetup.sampleRate);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 1, latencyNumSamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
                
        engine.getAudioFileManager().releaseAllFiles();
        edit->getTempDirectory (false).deleteRecursively();
    }
}
