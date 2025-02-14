/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_RACKINSTANCE
TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("Rack instance wet/dry param automation")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        // Sin file must outlive everything!
        auto fileLength = 5_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        // Rack
        VolumeAndPanPlugin::Ptr volPanPlugin (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));

        Plugin::Array plugins;
        plugins.add (volPanPlugin);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        auto rackInstancePlugin = track->pluginList.insertPlugin (RackInstance::create (*rackType), 0);
        auto rackInstance = dynamic_cast<RackInstance*> (rackInstancePlugin.get());

        auto& wetCurve = rackInstance->wetGain->getCurve();
        wetCurve.addPoint (2.5_tp, 1.0f, 0.0);
        wetCurve.addPoint (2.5_tp, 0.0f, 0.0);

        // This allows time for the active automation to update.
        // It shouldn't be needed in the future.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1000);

        CHECK (wetCurve.getValueAt (2_tp) == doctest::Approx (1.0f));
        CHECK (wetCurve.getValueAt (3_tp) == doctest::Approx (0.0f));

        // Render clip
        {
            insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                            DeleteExistingClips::no);

            auto render = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));
            CHECK (test_utilities::getRMSLevel (render, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f));
        }

        // Internal plugin automation
        {
            rackInstance->wetGain->getCurve().clear();

            auto& volCurve = volPanPlugin->volParam->getCurve();
            volCurve.addPoint (2.5_tp, volCurve.getValueAt (0_tp), 0.0);
            volCurve.addPoint (2.5_tp, 0.0f, 0.0);

            auto render = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));
            CHECK (test_utilities::getRMSLevel (render, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f));
        }
    }
}
#endif

} // namespace::inline namespace engine

#endif //TRACKTION_UNIT_TESTS
