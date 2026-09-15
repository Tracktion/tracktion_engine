/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EXTERNAL_CONTROLLER

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ExternalController: palette hues map to distinct pad colour indices")
    {
        const int numPaletteColours = 18;

        for (int i = 0; i < numPaletteColours; ++i)
        {
            auto colour = juce::Colours::red.withHue ((float) i / (float) numPaletteColours).withSaturation (0.7f);
            CHECK_EQ (ExternalController::getPadColourIndex (colour, false), i + 1);
        }

        CHECK_EQ (ExternalController::getPadColourIndex (juce::Colours::transparentBlack, false), 0);
        CHECK_EQ (ExternalController::getPadColourIndex (juce::Colours::transparentBlack, true), 0);
        CHECK_EQ (ExternalController::getPadColourIndex (juce::Colours::blue, true), 1);
    }

    TEST_CASE ("ExternalController: control surface clip colour defaults to the clip's colour")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto track = getAudioTracks (*edit)[0];

        edit->getSceneList().ensureNumberOfScenes (1);
        auto slot = track->getClipSlotList().getClipSlots()[0];
        REQUIRE (slot != nullptr);

        auto clip = insertMIDIClip (*slot, { TimePosition(), TimePosition::fromSeconds (1.0) });
        REQUIRE (clip != nullptr);

        auto clipColour = juce::Colours::red.withHue (5.0f / 18.0f).withSaturation (0.7f);
        clip->setColour (clipColour);
        track->setColour (juce::Colours::red.withHue (11.0f / 18.0f).withSaturation (0.7f));

        CHECK (engine.getUIBehaviour().getClipColourForControlSurface (*clip) == clipColour);
    }
}

} // namespace tracktion::inline engine

#endif
