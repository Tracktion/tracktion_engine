
/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

#if ENGINE_UNIT_TESTS_SELECTABLE

#include "../utilities/tracktion_TestUtilities.h"


namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class SelectableTests   : public juce::UnitTest
{
public:
    SelectableTests()
        : juce::UnitTest ("Selectable", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];
        runSafeSelectableTests (engine);
    }

private:
    void runSafeSelectableTests (Engine& engine)
    {
        beginTest ("Safe selectable");

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];
        auto plugin = track->getVolumePlugin();

        auto safeEdit = makeSafeRef (*edit);
        auto safeTrack = makeSafeRef (*track);
        auto safePlugin = makeSafeRef (*plugin);

        expect (safeEdit.get() != nullptr);
        expect (safeTrack.get() != nullptr);
        expect (safePlugin.get() != nullptr);

        expect (safeEdit == edit.get());
        expect (safeTrack == track);
        expect (safePlugin.get() == plugin);

        edit->deleteTrack (safeTrack);
        expect (safeTrack.get() == nullptr);

        edit.reset();
        expect (safeEdit == nullptr);
        expect (safeTrack == nullptr);
        expect (safePlugin == nullptr);
    }
};

static SelectableTests selectableTests;

}} // namespace tracktion { inline namespace engine

#endif //ENGINE_UNIT_TESTS_SELECTABLE
