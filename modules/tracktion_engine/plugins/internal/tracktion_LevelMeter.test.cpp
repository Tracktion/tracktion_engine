/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LEVELMETERPLUGIN

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

struct LevelMeterPluginTestAccess
{
    static LevelMeasurer::Client& getControllerClient (LevelMeterPlugin& p)  { return p.controllerLevelClient; }
    static void setControllerTrack (LevelMeterPlugin& p, int t)              { p.controllerTrack = t; }
};

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("LevelMeterPlugin: timer callback survives >32 channel tracks")
    {
        // Regression test for beta issue #1096: loading a high-channel-count plugin
        // (e.g. SPARTA ambisonics) or audio file (>34 channels) onto a track caused
        // a stack-smashing crash in LevelMeterPlugin::timerCallback, where a fixed
        // float gains[32] buffer was overrun.

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto meter = track->getLevelMeterPlugin();
        REQUIRE (meter != nullptr);

        // Attach the edit to the ExternalControllerManager so that the timer-callback
        // path that reads from controllerLevelClient is exercised.
        auto& ecm = engine.getExternalControllerManager();
        ecm.setCurrentEdit (edit.get(), nullptr);
        REQUIRE (ecm.isAttachedToEdit (*edit));

        // Force the controllerTrack into a valid range so timerCallback runs its
        // body (skipping the lazy initialise path that depends on edit indexing).
        LevelMeterPluginTestAccess::setControllerTrack (*meter, 0);

        // Seed every one of 64 channels with a known, non-silent level via the
        // controllerLevelClient directly. This also grows numChannelsUsed to 64.
        constexpr int wideChannels = 64;
        auto& client = LevelMeterPluginTestAccess::getControllerClient (*meter);
        client.setNumChannelsUsed (wideChannels);

        const auto now = juce::Time::getApproximateMillisecondCounter();
        const float seededDb = -6.0f;
        for (int i = 0; i < wideChannels; ++i)
            client.updateAudioLevel (i, { now, seededDb });

        meter->timerCallback();

        // Every channel should now have been read+cleared (dB reset to -100).
        for (int i : { 0, 31, 32, 63 })
        {
            const auto clearedDb = client.getAndClearAudioLevel (i).dB;
            CHECK_MESSAGE (clearedDb < -50.0f,
                           "timerCallback should have processed channel ", i,
                           " (dB still at ", clearedDb, " — buffer was not grown to numChans)");
        }

        ecm.setCurrentEdit (nullptr, nullptr);
    }
}

} // namespace tracktion::inline engine

#endif
