
/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ENGINE_UNIT_TESTS_SELECTABLE

#include "../utilities/tracktion_TestUtilities.h"
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Selectable")
    {
        auto& engine = *Engine::getEngines()[0];

        SUBCASE ("Safe selectable")
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];
            auto plugin = track->getVolumePlugin();

            auto safeEdit = makeSafeRef (*edit);
            auto safeTrack = makeSafeRef (*track);
            auto safePlugin = makeSafeRef (*plugin);

            CHECK (safeEdit.get() != nullptr);
            CHECK (safeTrack.get() != nullptr);
            CHECK (safePlugin.get() != nullptr);

            CHECK (safeEdit == edit.get());
            CHECK (safeTrack == track);
            CHECK (safePlugin.get() == plugin);

            edit->deleteTrack (safeTrack);
            CHECK (safeTrack.get() == nullptr);

            edit.reset();
            CHECK (safeEdit == nullptr);
            CHECK (safeTrack == nullptr);
            CHECK (safePlugin == nullptr);
        }
    }
}

} // namespace tracktion::inline engine

#endif //ENGINE_UNIT_TESTS_SELECTABLE
