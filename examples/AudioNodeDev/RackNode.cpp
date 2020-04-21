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
}
