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
    TEST_CASE ("ExternalController: default clip colour index for control surfaces")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto track = getAudioTracks (*edit)[0];

        edit->getSceneList().ensureNumberOfScenes (1);
        auto slot = track->getClipSlotList().getClipSlots()[0];
        REQUIRE (slot != nullptr);

        auto clip = insertMIDIClip (*slot, { TimePosition(), TimePosition::fromSeconds (1.0) });
        REQUIRE (clip != nullptr);

        SUBCASE ("Palette hues map to distinct indexes and the track colour is ignored")
        {
            UIBehaviour ui;
            const auto numIndexes = ui.getNumClipColourIndexes();
            CHECK_EQ (numIndexes, 18);

            track->setColour (juce::Colours::red.withHue (11.0f / 18.0f).withSaturation (0.7f));

            for (int i = 0; i < numIndexes; ++i)
            {
                clip->setColour (juce::Colours::red.withHue ((float) i / (float) numIndexes).withSaturation (0.7f));
                CHECK_EQ (ui.getClipColourIndexForControlSurface (*clip), i + 1);
            }
        }

        SUBCASE ("The number of indexes comes from the UIBehaviour")
        {
            struct NineColourUIBehaviour : public UIBehaviour
            {
                int getNumClipColourIndexes() override  { return 9; }
            };

            NineColourUIBehaviour ui;

            clip->setColour (juce::Colours::red.withHue (4.0f / 9.0f).withSaturation (0.7f));
            CHECK_EQ (ui.getClipColourIndexForControlSurface (*clip), 5);

            clip->setColour (juce::Colours::red.withHue (8.0f / 9.0f).withSaturation (0.7f));
            CHECK_EQ (ui.getClipColourIndexForControlSurface (*clip), 9);
        }
    }
}

} // namespace tracktion::inline engine

#endif
