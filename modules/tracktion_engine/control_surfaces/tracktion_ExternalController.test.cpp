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

        SUBCASE ("The clip colour defaults to the clip's own colour")
        {
            UIBehaviour ui;
            const auto clipColour = juce::Colours::red.withHue (5.0f / 18.0f).withSaturation (0.7f);
            clip->setColour (clipColour);
            track->setColour (juce::Colours::red.withHue (11.0f / 18.0f).withSaturation (0.7f));

            CHECK (ui.getClipColourForControlSurface (*clip) == clipColour);
        }

        SUBCASE ("The default index follows the clip colour the UIBehaviour returns")
        {
            struct BlueClipsUIBehaviour : public UIBehaviour
            {
                juce::Colour getClipColourForControlSurface (const Clip&) override
                {
                    return juce::Colours::red.withHue (12.0f / 18.0f).withSaturation (0.7f);
                }
            };

            BlueClipsUIBehaviour ui;
            clip->setColour (juce::Colours::red.withHue (2.0f / 18.0f).withSaturation (0.7f));

            CHECK_EQ (ui.getClipColourIndexForControlSurface (*clip), 13);
        }
    }

    TEST_CASE ("ControlSurface: pad colour changes forward to padStateChanged by default")
    {
        auto& engine = *Engine::getEngines()[0];

        struct RecordingControlSurface : public ControlSurface
        {
            using ControlSurface::ControlSurface;

            void padStateChanged (int channel, int scene, int colourIdx, int state) override
            {
                lastCall = { channel, scene, colourIdx, state };
            }

            std::array<int, 4> lastCall { -1, -1, -1, -1 };
        };

        RecordingControlSurface cs (engine.getExternalControllerManager());
        cs.padColourStateChanged (2, 3, 7, juce::Colours::blue, 1);

        CHECK (cs.lastCall == std::array<int, 4> { 2, 3, 7, 1 });
    }
}

} // namespace tracktion::inline engine

#endif
